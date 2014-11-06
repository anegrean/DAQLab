//==============================================================================
//
// Title:		NIDAQmxManager.c
// Purpose:		A short description of the implementation.
//
// Created on:	22-7-2014 at 15:54:27 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//========================================================================================================================================================================================================
// Include files

#include "DAQLab.h" 		// include this first
#include <formatio.h> 
#include <userint.h>
#include "DAQLabModule.h"
#include "NIDAQmxManager.h"
#include "UI_NIDAQmxManager.h"
#include <nidaqmx.h>
#include "daqmxioctrl.h"
										 		 
//========================================================================================================================================================================================================
// Constants

	// NIDAQmxManager UI resource 
#define MOD_NIDAQmxManager_UI 		"./Modules/NIDAQmxManager/UI_NIDAQmxManager.uir"	
	// Out of memory message
#define OutOfMemoryMsg				"Error: Out of memory.\n\n"
	//----------------------------
	// DAQmx Task default settings
	//----------------------------
	// General task settings that use hw-timing IO
#define DAQmxDefault_Task_Timeout					5.0				// in [s]
#define DAQmxDefault_Task_SampleRate				100.0			// in [Hz]
#define DAQmxDefault_Task_MeasMode					MeasFinite
#define	DAQmxDefault_Task_NSamples					100
#define	DAQmxDefault_Task_BlockSize					4096			// must be a power of 2 !
#define DAQmxDefault_Task_RefClkFreq				10000000.0		// in [Hz]
	// CI Frequency
#define DAQmxDefault_CI_Frequency_Task_MinFreq		2				// in [Hz]
#define DAQmxDefault_CI_Frequency_Task_MaxFreq		100				// in [Hz]
#define DAQmxDefault_CI_Frequency_Task_Edge			Edge_Rising
#define DAQmxDefault_CI_Frequency_Task_MeasMethod	Meas_LowFreq1Ctr
#define DAQmxDefault_CI_Frequency_Task_MeasTime		0.001			// in [s]
#define DAQmxDefault_CI_Frequency_Task_Divisor		4
  	// CO Frequency     
#define DAQmxDefault_CO_Frequency_Task_hightime		0.0005   		// the time the pulse stays high [s]
#define DAQmxDefault_CO_Frequency_Task_lowtime     	0.0005			// the time the pulse stays low [s]
#define DAQmxDefault_CO_Frequency_Task_initialdel 	0			    // initial delay [s]
#define DAQmxDefault_CO_Frequency_Task_idlestate    DAQmx_Val_Low   // terminal state at rest, pulses switch this to the oposite state


	// DAQmx task settings tab names
#define DAQmxAITaskSetTabName						"AI"
#define DAQmxAOTaskSetTabName						"AO"
#define DAQmxDITaskSetTabName						"DI"
#define DAQmxDOTaskSetTabName						"DO"
#define DAQmxCITaskSetTabName						"CI"
#define DAQmxCOTaskSetTabName						"CO"
	// DAQmx AI and AO task settings tab indices
#define DAQmxAIAOTskSet_ChanTabIdx					0
#define DAQmxAIAOTskSet_SettingsTabIdx				1
#define DAQmxAIAOTskSet_TimingTabIdx				2
#define DAQmxAIAOTskSet_TriggerTabIdx				3

	// DAQmx CI and CO task settings tab indices

#define DAQmxCICOTskSet_SettingsTabIdx				0
#define DAQmxCICOTskSet_TimingTabIdx				1
#define DAQmxCICOTskSet_ClkTabIdx					2  
#define DAQmxCICOTskSet_TriggerTabIdx				3

	// DAQmx error macro
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto DAQmxError; else

	// Shared error codes
#define WriteAODAQmx_Err_DataUnderflow 				-1

//========================================================================================================================================================================================================
// Types
typedef struct NIDAQmxManager 	NIDAQmxManager_type; 

// Ranges for DAQmx IO, stored as pairs of low value and high value, e.g. -10 V, 10 V or -1 mA, 1 mA
typedef struct {
	int 					Nrngs;   					// number of ranges
	double* 				low;  						// low value 
	double* 				high; 						// high value
} IORange_type;

//--------------------------
// DAQmx channel definitions
//--------------------------
// Channel types
typedef enum {
	
	Chan_None,
	//-------------------------------------------------------------------------------------------------------------------------------
	// 														Analog Input
	//-------------------------------------------------------------------------------------------------------------------------------
	Chan_AI_Voltage,								// Voltage measurement. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Voltage_RMS,		 					// Voltage RMS measurement. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Voltage_WithExcit,						// Voltage measurement with an excitation source. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Voltage_WithExcit_TEDS,					// Voltage measurement with a TEDS excitation source. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Voltage_TEDS,							// Voltage measurement using a TEDS (Transducer Electronic Data Sheet) sensor. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Current,								// Current measurement. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Current_RMS,		 					// Current RMS measurement. Multiple channels (list or range) per task allowed.
	
	Chan_AICurrent_TEDS,							// Current measurement using a TEDS (Transducer Electronic Data Sheet) sensor. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Temp_Thrmcpl,							// Temperature measurement using a thermocouple. Multiple channels (list or range) per task allowed. 
	
	Chan_AI_Temp_Thrmcpl_TEDS,						// Temperature measurement using a TEDS thermocouple. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Temp_RTD,								// Temperature measurement using an RTD. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Temp_RTD_TEDS,							// Temperature measurement using a TEDS RTD. Multiple channels (list or range) per task allowed.
						 
	Chan_AI_Temp_Thrmstr_Iex,   				   	// Temperature measurement using a thermistor with current excitation. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Temp_Thrmstr_Iex_TEDS,   				// Temperature measurement using a TEDS thermistor with current excitation. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Temp_Thrmstr_Vex,						// Temperature measurement using a thermistor with voltage excitation. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Temp_Thrmstr_Vex_TEDS,					// Temperature measurement using a TEDS thermistor with voltage excitation. Multiple channels (list or range) per task allowed. 
	
	Chan_AI_Temp_BuiltInSensor,						// Temperature measurement using a built in sensor. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Freq_ToVoltage,							// Frequency measurement using a frequency to voltage converter. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Resistance,								// Resistance measurement. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Resistance_TEDS,						// Resistance measurement using a TEDS sensor. Multiple channels (list or range) per task allowed.
	
	Chan_AI_StrainGage,								// Strain measurement. Multiple channels (list or range) per task allowed.
	
	Chan_AI_StrainGage_TEDS,						// Strain measurement using a TEDS sensor. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Strain_RosetteGage,						// Strain measurement using a rosette strain gage. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Force_Bridge_TwoPointLin,				// Force measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure force or load.
													// For sensors whose specifications do not provide a polynomial for scaling or a table of electrical and physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Force_Bridge_Table,						// Force measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure force or load.
													// For sensors whose specifications provide a table of electrical values and the corresponding physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Force_Bridge_Polynomial,				// Force measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure force or load.
													// For sensors whose specifications provide a polynomial to convert electrical values to physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Force_Bridge_TEDS,						// Force measurement using a TEDS bridge-based sensor. Multiple channels (list or range) per task allowed.
													
	Chan_AI_Force_IEPE,								// For measuring force using an IEPE force sensor. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Force_IEPE_TEDS,						// For measuring force using a TEDS IEPE force sensor. Multiple channels (list or range) per task allowed.
					  
	Chan_AI_Pressure_Bridge_TwoPointLin,			// Pressure measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure pressure or load.
													// For sensors whose specifications do not provide a polynomial for scaling or a table of electrical and physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Pressure_Bridge_Table,					// Pressure measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure pressure or load.
													// For sensors whose specifications provide a table of electrical values and the corresponding physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Pressure_Bridge_Polynomial,				// Pressure measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure pressure or load.
													// For sensors whose specifications provide a polynomial to convert electrical values to physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Pressure_Bridge_TEDS,					// Pressure measurement using a TEDS bridge-based sensor.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Torque_Bridge_TwoPointLin,				// Torque measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure torque.
													// For sensors whose specifications do not provide a polynomial for scaling or a table of electrical and physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Torque_Bridge_Table,					// Torque measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure torque.
													// For sensors whose specifications provide a table of electrical values and the corresponding physical values.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Torque_Bridge_Polynomial,				// Torque measurement using a bridge-based sensor. Channel(s) use a Wheatstone bridge to measure torque.
													// For sensors whose specifications provide a polynomial to convert electrical values to physical values.
													// Multiple channels (list or range) per task allowed.
											
	Chan_AI_Torque_Bridge_TEDS,						// Torque measurement using a TEDS bridge-based sensor.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Bridge,									// For measuring voltage ratios from a Wheatstone bridge. Use this instance with bridge-based sensors that measure phenomena 
													// other than strain, force, pressure, or torque, or that scale data to physical units NI-DAQmx does not support.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Bridge_TEDS,							// For measuring voltage ratios from a TEDS Wheatstone bridge. Use this instance with bridge-based sensors that measure phenomena 
													// other than strain, force, pressure, or torque, or that scale data to physical units NI-DAQmx does not support.
													// Multiple channels (list or range) per task allowed.
													
	Chan_AI_Acceleration,							// For measuring acceleration using an accelerometer. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Acceleration_TEDS,						// For measuring acceleration using a TEDS accelerometer. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Velocity_IEPE,							// For measuring velocity using an IEPE velocity sensor. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Microphone,								// For measuring sound pressure using a microphone. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Microphone_TEDS,						// For measuring sound pressure using a TEDS microphone. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Position_LVDT,							// For measuring linear position using LVDT. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Position_LVDT_TEDS,						// For measuring linear position using TEDS LVDT. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Position_RVDT,							// For measuring angular position using RVDT. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Position_RVDT_TEDS,						// For measuring angular position using TEDS RVDT. Multiple channels (list or range) per task allowed.
	
	Chan_AI_Position_EddyCurrProxProbe,				// For measuring linear position using an eddy current proximity probe. Multiple channels (list or range) per task allowed.
	//-------------------------------------------------------------------------------------------------------------------------------
	// 														Analog Output
	//-------------------------------------------------------------------------------------------------------------------------------
	Chan_AO_Voltage,								// Voltage output. Multiple channels (list or range) per task allowed.
	
	Chan_AO_Current,								// Current output. Multiple channels (list or range) per task allowed.
	
	Chan_AO_Function,								// Function generation. Multiple channels (list or range) per task allowed.
	//-------------------------------------------------------------------------------------------------------------------------------
	// 														Digital Input
	//-------------------------------------------------------------------------------------------------------------------------------
	Chan_DI,										// Digital input. Multiple channels (list or range) per task allowed.
	//-------------------------------------------------------------------------------------------------------------------------------
	// 														Digital Output
	//-------------------------------------------------------------------------------------------------------------------------------
	Chan_DO,										// Digital output. Multiple channels (list or range) per task allowed.
	//-------------------------------------------------------------------------------------------------------------------------------
	// 														Counter Input
	//-------------------------------------------------------------------------------------------------------------------------------
	Chan_CI_Frequency,								// Measures the frequency of a digital signal. Only one counter per task is allowed. Use separate tasks for multiple counters.   
	
	Chan_CI_Period,									// Measures the period of a digital signal. Only one counter per task is allowed. Use separate tasks for multiple counters.
	
	Chan_CI_EdgeCount,								// Counts the number of rising or falling edges of a digital signal. 
													// Only one counter per task is allowed. Use separate tasks for multiple counters. 
	
	Chan_CI_Pulse_Width,							// Measure the width of a digital pulse. Only one counter per task is allowed. Use separate tasks for multiple counters.  
	
	Chan_CI_Pulse_Frequency,						// Measures the frequency and duty cycle of a digital pulse. Only one counter per task is allowed. Use separate tasks for multiple counters.  
	
	Chan_CI_Pulse_Time,								// Measures the high-time and low-time of a digital pulse. Only one counter per task is allowed. Use separate tasks for multiple counters.  
	
	Chan_CI_Pulse_Ticks,							// Measures the high-ticks and low-ticks of a digital pulse in the chosen timebase.
													// Only one counter per task is allowed. Use separate tasks for multiple counters.
	
	Chan_CI_SemiPeriod,								// Measure the time between state transitions of a digital signal. Only one counter per task is allowed. Use separate tasks for multiple counters.   
	
	Chan_CI_TwoEdgeSeparation,						// Measures the amount of time between the rising or falling edge of one digital signal and the rising or falling edge of another digital signal.
													// Only one counter per task is allowed. Use separate tasks for multiple counters.
	
	Chan_CI_LinearEncoder,							// Measures linear position using a linear encoder. Only one counter per task is allowed. Use separate tasks for multiple counters.
	
	Chan_CI_AngularEncoder,							// Measures angular position using an angular encoder. Only one counter per task is allowed. Use separate tasks for multiple counters.
	
	Chan_CI_GPSTimestamp,							// Takes a timestamp and synchronizes the counter to a GPS receiver. Only one counter per task is allowed. Use separate tasks for multiple counters.
	//-------------------------------------------------------------------------------------------------------------------------------
	// 														Counter Output
	//-------------------------------------------------------------------------------------------------------------------------------
	Chan_CO_Pulse_Frequency,						// Generates digital pulses defined by frequency and duty cycle. Multiple counters (list or range) per task allowed.
	
	Chan_CO_Pulse_Time,								// Generates digital pulses defined by the amount of time the pulse is at a high state and the amount of time the pulse is at a low state. 
	
	Chan_CO_Pulse_Ticks								// Generates digital pulses defined by the the number of timebase ticks that the pulse is at a high state and the number of timebase ticks that the pulse is at a low state.
	
} Channel_type;

// AI
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/ai0
	ListType				supportedMeasTypes;			// List of int type. Indicates the measurement types supported by the channel.
	int						terminalCfg;				// List of bitfield terminal configurations supported by the channel.
	IORange_type* 			Vrngs;      				// Ranges for voltage input  [V]
	IORange_type* 			Irngs;      				// Ranges for current input  [A]
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} AIChannel_type;

// AO
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/ao0
	ListType				supportedOutputTypes;		// List of int type. Indicates the output types supported by the channel.
	int						terminalCfg;				// List of bitfield terminal configurations supported by the channel.
	IORange_type* 			Vrngs;      				// Ranges for voltage output  [V]
	IORange_type* 			Irngs;      				// Ranges for current output  [A]
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} AOChannel_type;

// DI Line
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/port0/line0
	ListType				sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital input.
	BOOL					sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital input physical channel.
	BOOL					changeDetectSupported;		// Indicates if the change detection timing type is supported for the digital input physical channel.
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} DILineChannel_type;

// DI Port
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/port0
	ListType				sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital input.
	BOOL					sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital input physical channel.
	BOOL					changeDetectSupported;		// Indicates if the change detection timing type is supported for the digital input physical channel.
	unsigned int			portWidth;					// Indicates in bits the width of digital input port.
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} DIPortChannel_type;

// DO Line
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/port0/line0
	BOOL					sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital output physical channel.
	ListType				sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital output.
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} DOLineChannel_type;

// DO Port
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/port0
	BOOL					sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital output physical channel.
	ListType				sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital output.
	unsigned int			portWidth;					// Indicates in bits the width of digital output port.
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} DOPortChannel_type;

// CI
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/ctr0
	ListType				supportedMeasTypes;			// Indicates the measurement types supported by the channel.
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} CIChannel_type;

// CO
typedef struct {
	char* 					physChanName;				// Physical channel name, e.g. dev1/ctr0
	ListType				supportedOutputTypes;		// Indicates the output types supported by the channel.
	BOOL					inUse;						// True if channel is in use by a Task, False otherwise
} COChannel_type;

// DAQ device parameters
typedef struct {
	char*        			name;    	   				// Device name.
	char*        			type;    	   				// Indicates the product name of the device.
	unsigned int			productNum;					// unique product number
	unsigned int			serial;       				// device serial number
	double       			AISCmaxrate;  				// Single channel AI max rate   [Hz]
	double       			AIMCmaxrate;  				// Multiple channel AI max rate [Hz]
	double       			AIminrate;    				// AI minimum rate              [Hz]
	double       			AOmaxrate;    				// AO maximum rate              [Hz]
	double       			AOminrate;    				// AO minimum rate              [Hz]
	double       			DImaxrate;    				// DI maximum rate              [Hz]
	double       			DOmaxrate;    				// DO maximum rate              [Hz]
	ListType     			AIchan;       				// List of AIChannel_type*
	ListType     			AOchan;	   					// List of AOChannel_type*
	ListType     			DIlines;      				// List of DILineChannel_type*
	ListType     			DIports;      				// List of DIPortChannel_type*
	ListType     			DOlines;      				// List of DOLineChannel_type*
	ListType     			DOports;      				// List of DOPortChannel_type*
	ListType     			CIchan;	   					// List of CIChannel_type*
	ListType     			COchan;	   					// List of COChannel_type*
	BOOL					AnalogTriggering;			// Indicates if the device supports analog triggering.
	BOOL					DigitalTriggering;			// Indicates if the device supports digital triggering.
	int						AITriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_AI_TrigUsage device attribute for bitfield info.
	int						AOTriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_AO_TrigUsage device attribute for bitfield info.
	int						DITriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_DI_TrigUsage device attribute for bitfield info.
	int						DOTriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_DO_TrigUsage device attribute for bitfield info.
	int						CITriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_CI_TrigUsage device attribute for bitfield info.
	int						COTriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_CO_TrigUsage device attribute for bitfield info.
	ListType				AISupportedMeasTypes;		// List of int type. Indicates the measurement types supported by the physical channels of the device for analog input.
	ListType				AOSupportedOutputTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for analog output.
	ListType				CISupportedMeasTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for counter input.
	ListType				COSupportedOutputTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for counter output.
} DevAttr_type;

// Terminal type bit fields
typedef enum {
	Terminal_Bit_None 		= 0, 							// no terminal 
	Terminal_Bit_Diff 		= DAQmx_Val_Bit_TermCfg_Diff,	// differential 
	Terminal_Bit_RSE 		= DAQmx_Val_Bit_TermCfg_RSE ,	// referenced single-ended 
	Terminal_Bit_NRSE 		= DAQmx_Val_Bit_TermCfg_NRSE,	// non-referenced single-ended
	Terminal_Bit_PseudoDiff = 
		DAQmx_Val_Bit_TermCfg_PseudoDIFF					// pseudo-differential
} TerminalBitField_type;

// Terminal type
typedef enum {
	Terminal_None 			= 0, 						// no terminal 
	Terminal_Diff 			= DAQmx_Val_Diff,			// differential 
	Terminal_RSE 			= DAQmx_Val_RSE,			// referenced single-ended 
	Terminal_NRSE 			= DAQmx_Val_NRSE,			// non-referenced single-ended
	Terminal_PseudoDiff 	= DAQmx_Val_PseudoDiff		// pseudo-differential
} Terminal_type;

// Trigger types
typedef enum {
	Trig_None,
	Trig_DigitalEdge,
	Trig_DigitalPattern,
	Trig_AnalogEdge,
	Trig_AnalogWindow,
} Trig_type;

typedef enum {
	TrigWnd_Entering		= DAQmx_Val_EnteringWin,	// Entering window
	TrigWnd_Leaving			= DAQmx_Val_LeavingWin		// Leaving window
} TrigWndCond_type;										// Window trigger condition for analog triggering.

typedef enum {
	TrigSlope_Rising		=  DAQmx_Val_Rising,
	TrigSlope_Falling		=  DAQmx_Val_Falling
} TrigSlope_type;									// For analog and digital edge trigger type

typedef enum {
	SampClockEdge_Rising	=  DAQmx_Val_Rising,
	SampClockEdge_Falling   =  DAQmx_Val_Falling
} SampClockEdge_type;								// Sampling clock active edge.

// Measurement mode
typedef enum{
	MeasFinite				= DAQmx_Val_FiniteSamps,
	MeasCont				= DAQmx_Val_ContSamps
} MeasMode_type; 

typedef struct {
	MeasMode_type 			measMode;      				// Measurement mode: finite or continuous.
	double       			sampleRate;    				// Sampling rate in [Hz] if device task settings is used as reference.
	size_t        			nSamples;	    			// Total number of samples to be acquired in case of a finite recording if device task settings is used as reference.
	size_t*					refNSamples;				// Points to either device or VChan-based number of samples to acquire. By default points to device.
	double*					refSampleRate;				// Points to either device or VChan-based sampling rate. By default points to device.
	size_t        			blockSize;     				// Number of samples for reading after which callbacks are called.
	char*         			sampClkSource; 				// Sample clock source if NULL then OnboardClock is used, otherwise the given clock
	SampClockEdge_type 		sampClkEdge;   				// Sample clock active edge.
	char*         			refClkSource;  				// Reference clock source used to sync internal clock, if NULL internal clock has no reference clock. 
	double        			refClkFreq;    				// Reference clock frequency if such a clock is used.
	BOOL					useRefNSamples;				// The number of samples to be acquired by this module is taken from the connecting VChan.
	BOOL					useRefSamplingRate;			// The sampling rate used by this module is taken from the connecting VChan.
	int						freqCtrlID;					// Timing setting control copy IDs
	int						dutycycleCtrlID;			// Timing setting control copy IDs
	int						timehighCtrlID;				// Timing setting control copy IDs
	int						timelowCtrlID;				// Timing setting control copy IDs
	int						tickshighCtrlID;			// Timing setting control copy IDs
	int						tickslowCtrlID;				// Timing setting control copy IDs
	int						idlestateCtrlID;			// Timing setting control copy IDs
	int						initdelayCtrlID;			// Timing setting control copy IDs
} TaskTiming_type;

	
typedef struct {
	Trig_type 				trigType;					// Trigger type.
	char*     				trigSource;   				// Trigger source.
	double*					samplingRate;				// Reference to task sampling rate in [Hz]
	TrigSlope_type			slope;     					// For analog and digital edge trig type.
	double   				level; 						// For analog edge trigger.
	double    				wndTop; 	   				// For analog window trigger.
	double   				wndBttm;   					// For analog window trigger.
	TrigWndCond_type		wndTrigCond;  				// For analog window trigger.
	size_t					nPreTrigSamples;			// For reference type trigger. Number of pre-trigger samples to acquire.
	int						trigSlopeCtrlID;			// Trigger setting control copy IDs
	int						trigSourceCtrlID;			// Trigger setting control copy IDs
	int						preTrigDurationCtrlID;		// Trigger setting control copy IDs
	int						preTrigNSamplesCtrlID;		// Trigger setting control copy IDs
	int						levelCtrlID;				// Trigger setting control copy IDs
	int						windowTrigCondCtrlID;		// Trigger setting control copy IDs
	int						windowTopCtrlID;			// Trigger setting control copy IDs
	int						windowBottomCtrlID;			// Trigger setting control copy IDs
} TaskTrig_type;



typedef struct ChanSet	ChanSet_type;
typedef void (*DiscardChanSetFptr_type) (ChanSet_type** chanSet);
struct ChanSet {
	//DATA
	char*						name;					// Full physical channel name used by DAQmx, e.g. Dev1/ai1 
	Channel_type				chanType;				// Channel type.
	int							chanPanHndl;			// Panel handle where channel properties are adjusted
	TaskHandle					taskHndl;				// Only for counter type channels. Each counter requires a separate task.
	TaskTrig_type* 				startTrig;     			// Only for counter type channels. Task start trigger type. If NULL then there is no start trigger.
	TaskTrig_type* 				referenceTrig;     		// Only for counter type channels. Task reference trigger type. If NULL then there is no reference trigger.
	SourceVChan_type*			srcVChan;				// Source VChan assigned to input type physical channels. Otherwise NULL.
	SinkVChan_type*				sinkVChan;				// Sink VChan assigned to output type physical channels. Otherwise NULL.
	BOOL						onDemand;				// If TRUE, the channel is updated/read out using software-timing.
	// METHODS
	DiscardChanSetFptr_type		discardFptr;			// Function pointer to discard specific channel data that is derived from this base class.					
};

// Analog Input & Output channel type settings
typedef struct {
	ChanSet_type			baseClass;					// Channel settings common to all channel types.
	AIChannel_type*			aiChanAttr;					// If AI channel, this points to channel attributes, otherwise this is NULL.
	AOChannel_type*			aoChanAttr;					// If AO channel, this points to channel attributes, otherwise this is NULL.
	double        			Vmax;      					// Maximum voltage [V]
	double        			Vmin;       				// Minimum voltage [V]
	Terminal_type 			terminal;   				// Terminal type 	
} ChanSet_AIAO_Voltage_type;

typedef enum {
	ShuntResistor_Internal 	= DAQmx_Val_Internal,		// Use the internal shunt resistor for current measurements.
	ShuntResistor_External 	= DAQmx_Val_External		// Use an external shunt resistor for current measurement.
} ShuntResistor_type;

typedef struct {
	ChanSet_type			baseClass;					// Channel settings common to all channel types.
	double        			Imax;      					// Maximum voltage [A]
	double        			Imin;       				// Minimum voltage [A]
	ShuntResistor_type		shuntResistorType;
	double					shuntResistorValue;			// If shuntResistorType = ShuntResistor_External, this gives the value of the shunt resistor in [Ohms] 
	Terminal_type 			terminal;   				// Terminal type 	
} ChanSet_AIAO_Current_type;


// Digital Line/Port Input & Digital Line/Port Output channel type settings
typedef struct {
	ChanSet_type			baseClass;					// Channel settings common to all channel types.
	BOOL 					invert;    					// invert polarity (for ports the polarity is inverted for all lines
} ChanSet_DIDO_type;

// Counter Input for Counting Edges, channel type settings
typedef struct {
	ChanSet_type			baseClass;					// Channel settings common to all channel types.
	BOOL 					ActiveEdge; 				// 0 - Falling, 1 - Rising
	int 					initialCount;				// value to start counting from
	enum {
		C_UP, 
		C_DOWN, 
		C_EXT
	} 						countDirection; 			// count direction, C_EXT = externally controlled
} CIEdgeChanSet_type;

// Counter Input for measuring the frequency of a digital signal
typedef enum {
	Edge_Rising				=  DAQmx_Val_Rising,
	Edge_Falling			=  DAQmx_Val_Falling
} Edge_type;

typedef enum {
	Meas_LowFreq1Ctr		= DAQmx_Val_LowFreq1Ctr,	// Use one counter that uses a constant timebase to measure the input signal.
	Meas_HighFreq2Ctr		= DAQmx_Val_HighFreq2Ctr,	// Use two counters, one of which counts pulses of the signal to measure during the specified measurement time.
	Meas_LargeRng2Ctr		= DAQmx_Val_LargeRng2Ctr    // Use one counter to divide the frequency of the input signal to create a lower-frequency signal 
						  								// that the second counter can more easily measure.
} MeasMethod_type;

typedef struct {
	ChanSet_type			baseClass;					// Channel settings common to all channel types.
	double        			freqMax;      				// Maximum frequency expected to be measured [Hz]
	double        			freqMin;       				// Minimum frequency expected to be measured [Hz]
	Edge_type				edgeType;					// Specifies between which edges to measure the frequency or period of the signal.
	MeasMethod_type			measMethod;					// The method used to calculate the period or frequency of the signal. 
	double					measTime;					// The length of time to measure the frequency or period of a digital signal, when measMethod is DAQmx_Val_HighFreq2Ctr.
														// Measurement accuracy increases with increased measurement time and with increased signal frequency.
														// Caution: If you measure a high-frequency signal for too long a time, the count register could roll over, resulting in an incorrect measurement. 
	unsigned int			divisor;					// The value by which to divide the input signal, when measMethod is DAQmx_Val_LargeRng2Ctr. The larger this value, the more accurate the measurement, 
														// but too large a value can cause the count register to roll over, resulting in an incorrect measurement.
	char*					freqInputTerminal;			// If other then default terminal assigned to the counter, otherwise this is NULL.
	TaskTiming_type*		timing;						// Task timing info.
} ChanSet_CI_Frequency_type;


// Counter Output channel type settings
typedef struct {
	ChanSet_type			baseClass;					// Channel settings common to all channel types.
	double 					frequency;   				// 
	double 					dutycycle;    				// 
	int						units;
	int 					highticks;   				// optional
	int 					lowticks;    				// 
	double 					hightime;   				// the time the pulse stays high [s]
	double 					lowtime;    				// the time the pulse stays low [s]
	double 					initialdel; 				// initial delay [s]
	BOOL 					idlestate;    				// LOW = 0, HIGH = 1 - terminal state at rest, pulses switch this to the oposite state
	TaskTiming_type*		timing;						// Task timing info.   
} ChanSet_CO_type;

	
//--------------------
// DAQmx task settings
//--------------------
typedef enum {
	DAQmxAcquire,
	DAQmxGenerate
} DAQmxIO_type;

typedef enum {
	DAQmxAnalog,
	DAQmxDigital,
	DAQmxCounter,
	DAQmxTEDS
} DAQmxIOMode_type;										// not used

typedef enum {
	DAQmxDigLines,
 	DAQmxDigPorts
} DAQmxDigChan_type;

// Used for continuous AO streaming
typedef struct {
	size_t        			writeblock;        			// Size of writeblock is IO_block_size
	size_t        			numchan;           			// Number of output channels
	
	float64**     			datain;            
	float64**     			databuff;
	float64*      			dataout;					// Array length is writeblock * numchan used for DAQmx write call, data is grouped by channel
	SinkVChan_type**		sinkVChans;					// Array of SinkVChan_type*
	
	size_t*       			datain_size; 
	size_t*      			databuff_size;
	
	size_t*       			idx;						// Array of pointers with index for each channel from which to continue writing data (not used yet)  
	size_t*       			datain_repeat;	 			// Number of times to repeat the an entire data packet
	size_t*      			datain_remainder;  			// Number of elements from the beginning of the data packet to still generate, 
									 					// WARNING: this does not apply when looping is ON. In this case when receiving a new data packet, a full cycle is generated before switching.
	
	BOOL*         			datain_loop;	   			// Data will be looped regardless of repeat value until new data packet is provided
} WriteAOData_type;

// AI and AO
typedef struct {
	int						panHndl;					// Panel handle to task settings.
	TaskHandle				taskHndl;					// DAQmx task handle for hw-timed AI or AO.
	ListType 				chanSet;     				// Channel settings. Of ChanSet_type*
	double        			timeout;       				// Task timeout [s]
	TaskTrig_type* 			startTrig;     				// Task start trigger type. If NULL then there is no start trigger.
	TaskTrig_type* 			referenceTrig;     			// Task reference trigger type. If NULL then there is no reference trigger.
	WriteAOData_type*		writeAOData;				// Used for continuous AO streaming 
	TaskTiming_type*		timing;						// Task timing info
} AIAOTaskSet_type;

// DI and DO
typedef struct {
	int						panHndl;					// Panel handle to task settings.
	TaskHandle				taskHndl;					// DAQmx task handle for hw-timed DI and DO.
	ListType 				chanSet;     				// Channel settings. Of ChanSet_type*
	double        			timeout;       				// Task timeout [s]
	TaskTrig_type* 			startTrig;     				// Task start trigger type. If NULL then there is no start trigger.
	TaskTrig_type* 			referenceTrig;     			// Task reference trigger type. If NULL then there is no reference trigger.
	TaskTiming_type*		timing;						// Task timing info
} DIDOTaskSet_type;

// CI
typedef struct {
	// DAQmx task handles are specified per counter channel within chanTaskSet
	int						panHndl;					// Panel handle to task settings.
	TaskHandle				taskHndl;					// DAQmx task handle for hw-timed CI
	ListType 				chanTaskSet;   				// Channel and task settings. Of ChanSet_type*
	double        			timeout;       				// Task timeout [s]
} CITaskSet_type;

// CO
typedef struct {
	// DAQmx task handles are specified per counter channel chanTaskSet
	int						panHndl;					// Panel handle to task settings.
	TaskHandle				taskHndl;					// DAQmx task handle for hw-timed CO.
	ListType 				chanTaskSet;     			// Channel and task settings. Of ChanSet_type*
	double        			timeout;       				// Task timeout [s]
} COTaskSet_type;

// DAQ Task definition
typedef struct {
	NIDAQmxManager_type*	niDAQModule;				// Reference to the module that owns the DAQmx device. 
	int						devPanHndl;					// Panel handle to DAQmx task settings.
	DevAttr_type*			attr;						// Device to be used during IO task
	TaskControl_type*   	taskController;				// Task Controller for the DAQmx module
	AIAOTaskSet_type*		AITaskSet;					// AI task settings
	AIAOTaskSet_type*		AOTaskSet;					// AO task settings
	DIDOTaskSet_type*		DITaskSet;					// DI task settings
	DIDOTaskSet_type*		DOTaskSet;					// DO task settings
	CITaskSet_type*			CITaskSet;					// CI task settings
	COTaskSet_type*			COTaskSet;					// CO task settings
} Dev_type;

//========================================================================================================================================================================================================
// Module implementation

struct NIDAQmxManager {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 		baseClass;
	
	// DATA
	
	ListType            	DAQmxDevices;   			// list of Dev_type* for each DAQmx device 
	
		//-------------------------
		// UI
		//-------------------------
	
	int						mainPanHndl;
	int						taskSetPanHndl;
	int						devListPanHndl;
	int						menuBarHndl;
	int						deleteDevMenuItemID;
	
	
	// METHODS 
	
};

//========================================================================================================================================================================================================
// Static global variables

static int	 currDev = -1;        // currently selected device from the DAQ table

//========================================================================================================================================================================================================
// Static functions

static Dev_type* 					init_Dev_type 							(NIDAQmxManager_type* niDAQModule, DevAttr_type** attr, char taskControllerName[]);
static void 						discard_Dev_type						(Dev_type** a);

static int 							init_DevList 							(ListType devlist, int panHndl, int tableCtrl);
static void 						empty_DevList 							(ListType devList); 

	// device attributes management
static DevAttr_type* 				init_DevAttr_type						(void); 
static void 						discard_DevAttr_type					(DevAttr_type** a);
static DevAttr_type* 				copy_DevAttr_type						(DevAttr_type* attr);

	// AI channel
static AIChannel_type* 				init_AIChannel_type						(void); 
static void 						discard_AIChannel_type					(AIChannel_type** a);
int CVICALLBACK 					DisposeAIChannelList	 				(size_t index, void* ptrToItem, void* callbackData);
static AIChannel_type* 				copy_AIChannel_type						(AIChannel_type* channel);
static ListType						copy_AIChannelList						(ListType chanList);
static AIChannel_type*				GetAIChannel							(Dev_type* dev, char physChanName[]);

	// AO channel
static AOChannel_type* 				init_AOChannel_type						(void); 
static void 						discard_AOChannel_type					(AOChannel_type** a);
int CVICALLBACK 					DisposeAOChannelList 					(size_t index, void* ptrToItem, void*callbackData);
static AOChannel_type* 				copy_AOChannel_type						(AOChannel_type* channel);
static ListType						copy_AOChannelList						(ListType chanList);
static AOChannel_type*				GetAOChannel							(Dev_type* dev, char physChanName[]);
	
	// DI Line channel
static DILineChannel_type* 			init_DILineChannel_type					(void); 
static void 						discard_DILineChannel_type				(DILineChannel_type** a);
int CVICALLBACK 					DisposeDILineChannelList	 			(size_t index, void* ptrToItem, void* callbackData);
static DILineChannel_type* 			copy_DILineChannel_type					(DILineChannel_type* channel);
static ListType						copy_DILineChannelList					(ListType chanList);
static DILineChannel_type*			GetDILineChannel						(Dev_type* dev, char physChanName[]);

	// DI Port channel
static DIPortChannel_type* 			init_DIPortChannel_type					(void); 
static void 						discard_DIPortChannel_type				(DIPortChannel_type** a);
int CVICALLBACK 					DisposeDIPortChannelList 				(size_t index, void* ptrToItem, void* callbackData);
static DIPortChannel_type* 			copy_DIPortChannel_type					(DIPortChannel_type* channel);
static ListType						copy_DIPortChannelList					(ListType chanList);
static DIPortChannel_type*			GetDIPortChannel						(Dev_type* dev, char physChanName[]);

	// DO Line channel
static DOLineChannel_type* 			init_DOLineChannel_type					(void); 
static void 						discard_DOLineChannel_type				(DOLineChannel_type** a);
int CVICALLBACK 					DisposeDOLineChannelList	 			(size_t index, void* ptrToItem, void* callbackData);
static DOLineChannel_type* 			copy_DOLineChannel_type					(DOLineChannel_type* channel);
static ListType						copy_DOLineChannelList					(ListType chanList);
static DOLineChannel_type*			GetDOLineChannel						(Dev_type* dev, char physChanName[]);

	// DO Port channel
static DOPortChannel_type* 			init_DOPortChannel_type					(void); 
static void 						discard_DOPortChannel_type				(DOPortChannel_type** a);
int CVICALLBACK 					DisposeDOPortChannelList 				(size_t index, void* ptrToItem, void* callbackData);
static DOPortChannel_type* 			copy_DOPortChannel_type					(DOPortChannel_type* channel);
static ListType						copy_DOPortChannelList					(ListType chanList);
static DOPortChannel_type*			GetDOPortChannel						(Dev_type* dev, char physChanName[]);

	// CI channel
static CIChannel_type* 				init_CIChannel_type						(void); 
static void 						discard_CIChannel_type					(CIChannel_type** a);
int CVICALLBACK 					DisposeCIChannelList 					(size_t index, void* ptrToItem, void* callbackData);
static CIChannel_type* 				copy_CIChannel_type						(CIChannel_type* channel);
static ListType						copy_CIChannelList						(ListType chanList);
static CIChannel_type*				GetCIChannel							(Dev_type* dev, char physChanName[]);

	// CO channel
static COChannel_type* 				init_COChannel_type						(void); 
static void 						discard_COChannel_type					(COChannel_type** a);
int CVICALLBACK 					DisposeCOChannelList 					(size_t index, void* ptrToItem, void* callbackData);
static COChannel_type* 				copy_COChannel_type						(COChannel_type* channel);
static ListType						copy_COChannelList						(ListType chanList);
static COChannel_type*				GetCOChannel							(Dev_type* dev, char physChanName[]);

//-----------------
// Channel settings
//-----------------
	// channel settings base class
static void							init_ChanSet_type						(ChanSet_type* chanSet, char physChanName[], Channel_type chanType, DiscardChanSetFptr_type discardFptr);
static void							discard_ChanSet_type					(ChanSet_type** a);

	// AI and AO Voltage
static ChanSet_type* 				init_ChanSet_AIAO_Voltage_type 			(char physChanName[], Channel_type chanType, AIChannel_type* aiChanAttrPtr, AOChannel_type* aoChanAttrPtr);
static void							discard_ChanSet_AIAO_Voltage_type		(ChanSet_type** a);
static int 							ChanSetAIAOVoltage_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// DI and DO 
static ChanSet_type*				init_ChanSet_DIDO_type					(char physChanName[], Channel_type chanType);
static void							discard_ChanSet_DIDO_type				(ChanSet_type** a);

	// CI
static ChanSet_type* 				init_ChanSet_CI_Frequency_type 			(char physChanName[]);
static void 						discard_ChanSet_CI_Frequency_type 		(ChanSet_type** a);
	//CO
static ChanSet_type* 				init_ChanSet_CO_type 					(char physChanName[],Channel_type chanType);
static void 						discard_ChanSet_CO_type 				(ChanSet_type** a);

	// channels & property lists
static ListType						GetPhysChanPropertyList					(char chanName[], int chanProperty); 
static void							PopulateChannels						(Dev_type* dev);
static int							AddDAQmxChannel							(Dev_type* dev, DAQmxIO_type ioVal, DAQmxIOMode_type ioMode, int ioType, char chName[]);
static int 							RemoveDAQmxAIChannel_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int 							RemoveDAQmxAOChannel_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							RemoveDAQmxCIChannel_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int 			 				RemoveDAQmxCOChannel_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

	// IO mode/type
static ListType						GetSupportedIOTypes						(char devName[], int IOType);
static void 						PopulateIOMode 							(Dev_type* dev, int panHndl, int controlID, int ioVal);
static void 						PopulateIOType 							(Dev_type* dev, int panHndl, int controlID, int ioVal, int ioMode); 

	// ranges
static IORange_type* 				init_IORange_type						(void);
static void 						discard_IORange_type					(IORange_type** a);
static IORange_type*				copy_IORange_type						(IORange_type* IOrange);
static IORange_type*				GetIORanges								(char devName[], int rangeType);

//----------------
// VChan Callbacks
//----------------
static void							VChanConnected							(VChan_type* self, VChan_type* connectedVChan);
static void							VChanDisconnected						(VChan_type* self, VChan_type* disconnectedVChan);

//--------------------
// DAQmx task triggers
//--------------------
	// task trigger
static TaskTrig_type*				init_TaskTrig_type						(double* samplingRate);
static void							discard_TaskTrig_type					(TaskTrig_type** a);

//------------------
// DAQmx task timing
//------------------
static TaskTiming_type*				init_TaskTiming_type					(void);
static void							discard_TaskTiming_type					(TaskTiming_type** a);

//--------------------
// DAQmx task settings
//--------------------
	// AI and AO task settings
static AIAOTaskSet_type*			init_AIAOTaskSet_type					(void);
static void							discard_AIAOTaskSet_type				(AIAOTaskSet_type** a);
static int 							AI_Settings_TaskSet_CB			 		(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							AI_Timing_TaskSet_CB			 		(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							AO_Settings_TaskSet_CB	 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int 							AO_Timing_TaskSet_CB	 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

	// AO continuous streaming data structure
static WriteAOData_type* 			init_WriteAOData_type					(Dev_type* dev);
static void							discard_WriteAOData_type				(WriteAOData_type** a);

	// DI and DO task settings
static DIDOTaskSet_type*			init_DIDOTaskSet_type					(void);
static void							discard_DIDOTaskSet_type				(DIDOTaskSet_type** a);

	// CI task settings
static CITaskSet_type*				init_CITaskSet_type						(void);
static void							discard_CITaskSet_type					(CITaskSet_type** a);

	// CO task settings
static COTaskSet_type*				init_COTaskSet_type						(void);
static void							discard_COTaskSet_type					(COTaskSet_type** a);

//-----------------------
// DAQmx tasks management
//-----------------------
static FCallReturn_type*			ConfigDAQmxDevice						(Dev_type* dev); 
static FCallReturn_type*			ConfigDAQmxAITask 						(Dev_type* dev);
static FCallReturn_type*			ConfigDAQmxAOTask 						(Dev_type* dev);
static FCallReturn_type*			ConfigDAQmxDITask 						(Dev_type* dev);
static FCallReturn_type*			ConfigDAQmxDOTask 						(Dev_type* dev);
static FCallReturn_type*			ConfigDAQmxCITask 						(Dev_type* dev);
static FCallReturn_type*			ConfigDAQmxCOTask 						(Dev_type* dev);
static void 						DisplayLastDAQmxLibraryError 			(void);
	// Clears all DAQmx Tasks defined for the device
static int 							ClearDAQmxTasks 						(Dev_type* dev); 
	// Stops all DAQmx Tasks defined for the device
static FCallReturn_type*			StopDAQmxTasks 							(Dev_type* dev); 
	// Checks if all DAQmx Tasks are done
static BOOL							DAQmxTasksDone							(Dev_type* dev);
	// Starts all DAQmx Tasks defined for the device
	// Note: Tasks that have a HW-trigger defined will start before tasks that do not have any HW-trigger defined. This allows the later type of
	// tasks to trigger the HW-triggered tasks. If all tasks are started successfully, it returns NULL, otherwise returns a negative value with DAQmx error code and error string combined.
static FCallReturn_type*			StartAllDAQmxTasks						(Dev_type* dev);

//---------------------
// DAQmx task callbacks
//---------------------
	// AI
int32 CVICALLBACK 					AIDAQmxTaskDataAvailable_CB 			(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData);
int32 CVICALLBACK 					AIDAQmxTaskDone_CB 						(TaskHandle taskHandle, int32 status, void *callbackData);

	// AO
int32 CVICALLBACK 					AODAQmxTaskDataRequest_CB				(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData);
int32 CVICALLBACK 					AODAQmxTaskDone_CB 						(TaskHandle taskHandle, int32 status, void *callbackData);

	// DI
int32 CVICALLBACK 					DIDAQmxTaskDataAvailable_CB 			(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData);
int32 CVICALLBACK 					DIDAQmxTaskDone_CB 						(TaskHandle taskHandle, int32 status, void *callbackData);

	// DO
int32 CVICALLBACK 					DODAQmxTaskDataRequest_CB				(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData);
int32 CVICALLBACK 					DODAQmxTaskDone_CB 						(TaskHandle taskHandle, int32 status, void *callbackData);

	// CO
int32 CVICALLBACK 					CODAQmxTaskDone_CB 						(TaskHandle taskHandle, int32 status, void *callbackData);

//-------------------------------------
// DAQmx module and VChan data exchange
//-------------------------------------
	// AO
FCallReturn_type* 					WriteAODAQmx 							(Dev_type* dev); 

	// Output buffers
static BOOL							OutputBuffersFilled						(Dev_type* dev);
static FCallReturn_type* 			FillOutputBuffer						(Dev_type* dev); 

//--------------------------------------------
// Various DAQmx module managemement functions
//--------------------------------------------

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl);

static char* 						substr									(const char* token, char** idxstart);

static BOOL							ValidTaskControllerName					(char name[], void* dataPtr);

//---------------------------
// DAQmx module UI management
//---------------------------

static int 							DisplayPanels							(DAQLabModule_type* mod, BOOL visibleFlag); 

static int CVICALLBACK 				MainPan_CB								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			ManageDevPan_CB 						(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void CVICALLBACK 			DeleteDev_CB 							(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

	// trigger settings callbacks
static int CVICALLBACK 				TaskStartTrigType_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TaskReferenceTrigType_CB				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerSlope_CB	 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerLevel_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerSource_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerWindowType_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerWindowBttm_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerWindowTop_CB						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerPreTrigDuration_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static int CVICALLBACK 				TriggerPreTrigSamples_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// adjustment of the DAQmx task settings panel
static int 							DAQmxDevTaskSet_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

//-----------------------------------------
// NIDAQmx Device Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*			ConfigureTC								(TaskControl_type* taskControl, BOOL const* abortFlag);

static void							IterateTC								(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag);

static void							AbortIterationTC						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*			StartTC									(TaskControl_type* taskControl, BOOL const* abortFlag);

static FCallReturn_type*			DoneTC									(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*			StoppedTC								(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static void							DimTC									(TaskControl_type* taskControl, BOOL dimmed);

static FCallReturn_type* 			ResetTC 								(TaskControl_type* taskControl, BOOL const* abortFlag); 

static void				 			ErrorTC 								(TaskControl_type* taskControl, char* errorMsg);

static FCallReturn_type*			DataReceivedTC							(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag);

static FCallReturn_type*			ModuleEventHandler						(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);  

void GetCO_HiLo(double freq,double dutycycle,double* hightime,double* lowtime);

//========================================================================================================================================================================================================
// Global variables

ListType					devList			= 0;		// List of DAQ devices available of DevAttr_type. This will be updated every time init_DevList(ListType) is executed.

//========================================================================================================================================================================================================
// Global functions

//===============================================================================================================
//											Structure init/discard
//===============================================================================================================

//------------------------------------------------------------------------------
// NIDAQmxManager module
//------------------------------------------------------------------------------

/// HIFN  Allocates memory and initializes an NI DAQmx device.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed. 
DAQLabModule_type*	initalloc_NIDAQmxManager (DAQLabModule_type* mod, char className[], char instanceName[])
{
	NIDAQmxManager_type* nidaq;
	
	if (!mod) {
		nidaq = malloc (sizeof(NIDAQmxManager_type));
		if (!nidaq) return NULL;
	} else
		nidaq = (NIDAQmxManager_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&nidaq->baseClass, className, instanceName);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
		// METHODS
		
			// overriding methods
	nidaq->baseClass.Discard 		= discard_NIDAQmxManager;
			
	nidaq->baseClass.Load			= Load; 
	
	nidaq->baseClass.DisplayPanels	= DisplayPanels;
			
	//---------------------------
	// Child Level 1: NIDAQmxManager_type
	
		// DATA
	nidaq->DAQmxDevices				= ListCreate(sizeof(Dev_type*));
	if (!nidaq->DAQmxDevices) {discard_DAQLabModule((DAQLabModule_type**)&nidaq); return NULL;}
	
	nidaq->mainPanHndl				= 0;
	nidaq->menuBarHndl				= 0;
	nidaq->deleteDevMenuItemID		= 0;
	nidaq->taskSetPanHndl			= 0;
	nidaq->devListPanHndl			= 0;
	
								
		// METHODS
		
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) nidaq;
	else
		return NULL;
}

/// HIFN Discards NIDAQmxManager_type data but does not free the structure memory.
void discard_NIDAQmxManager (DAQLabModule_type** mod)
{
	NIDAQmxManager_type* 	nidaq 		= (NIDAQmxManager_type*) (*mod);
	size_t					nDAQDevs	= ListNumItems(nidaq->DAQmxDevices);
	Dev_type**				DAQDevPtrPtr;
	
	if (!nidaq) return;
	
	//------------------------------------------
	// discard NIDAQmxManager_type specific data
	//------------------------------------------
	
	// discard DAQmx device data
	for (size_t i = nDAQDevs; i ; i--) {
		DAQDevPtrPtr = ListGetPtrToItem(nidaq->DAQmxDevices, i);
		discard_Dev_type(DAQDevPtrPtr);
	}
	ListDispose(nidaq->DAQmxDevices);
	
	// discard UI
	if (nidaq->mainPanHndl) {DiscardPanel(nidaq->mainPanHndl); nidaq->mainPanHndl = 0;}
	if (nidaq->taskSetPanHndl) {DiscardPanel(nidaq->taskSetPanHndl); nidaq->taskSetPanHndl = 0;}
	if (nidaq->devListPanHndl) {DiscardPanel(nidaq->devListPanHndl); nidaq->devListPanHndl = 0;}
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}

//------------------------------------------------------------------------------
// DevAttr_type
//------------------------------------------------------------------------------

static DevAttr_type* init_DevAttr_type(void)
{
	int error = 0;
	
	DevAttr_type* a = malloc (sizeof(DevAttr_type));
	if (!a) return NULL;
	
	a -> name         			= NULL; // dynamically allocated string
	a -> type         			= NULL; // dynamically allocated string
	a -> productNum				= 0;	// unique product number
	a -> serial       			= 0;   	// device serial number
	a -> AISCmaxrate  			= 0;   	// Single channel AI max rate
	a -> AIMCmaxrate  			= 0;   	// Multiple channel AI max rate
	a -> AIminrate    			= 0;
	a -> AOmaxrate    			= 0;
	a -> AOminrate    			= 0;
	a -> DImaxrate    			= 0;
	a -> DOmaxrate				= 0;
	
	// init lists
	a -> AIchan					= 0;
	a -> AOchan					= 0;
	a -> DIlines				= 0;
	a -> DIports				= 0;
	a -> DOlines				= 0;
	a -> DOports				= 0;
	a -> CIchan					= 0;
	a -> COchan					= 0;
	
	// create list of channels
	nullChk(a -> AIchan   		= ListCreate (sizeof(AIChannel_type*)));
	nullChk(a -> AOchan	  		= ListCreate (sizeof(AOChannel_type*)));
	nullChk(a -> DIlines  		= ListCreate (sizeof(DILineChannel_type*)));
	nullChk(a -> DIports  		= ListCreate (sizeof(DIPortChannel_type*)));
	nullChk(a -> DOlines  		= ListCreate (sizeof(DOLineChannel_type*)));
	nullChk(a -> DOports  		= ListCreate (sizeof(DOPortChannel_type*)));
	nullChk(a -> CIchan   		= ListCreate (sizeof(CIChannel_type*)));
	nullChk(a -> COchan   		= ListCreate (sizeof(COChannel_type*)));
	
	// triggering
	a -> AnalogTriggering		= 0;
	a -> DigitalTriggering		= 0;
	
	a -> AITriggerUsage			= 0;
	a -> AOTriggerUsage			= 0;
	a -> DITriggerUsage			= 0;
	a -> DOTriggerUsage			= 0;
	a -> CITriggerUsage			= 0;
	a -> COTriggerUsage			= 0;
	
	// supported IO types list init
	a -> AISupportedMeasTypes	= 0;
	a -> AOSupportedOutputTypes	= 0;
	a -> CISupportedMeasTypes	= 0;
	a -> COSupportedOutputTypes	= 0;
	
	return a;
Error:
	
	if (a->AIchan) 		ListDispose(a->AIchan);
	if (a->AOchan) 		ListDispose(a->AOchan);
	if (a->DIlines) 	ListDispose(a->DIlines);
	if (a->DIports) 	ListDispose(a->DIports);
	if (a->DOlines) 	ListDispose(a->DOlines);
	if (a->DOports) 	ListDispose(a->DOports);
	if (a->CIchan) 		ListDispose(a->CIchan);
	if (a->COchan) 		ListDispose(a->COchan);
	
	free(a);
	
	return NULL;
}

static void discard_DevAttr_type(DevAttr_type** a)
{
	if (!(*a)) return;
	
	OKfree((*a)->name);
	OKfree((*a)->type);
	
	// discard channel lists and free pointers within lists.
	if ((*a)->AIchan) 	ListApplyToEachEx ((*a)->AIchan, 1, DisposeAIChannelList, NULL); ListDispose((*a)->AIchan); 
	if ((*a)->AOchan) 	ListApplyToEachEx ((*a)->AOchan, 1, DisposeAOChannelList, NULL); ListDispose((*a)->AOchan);	  
	if ((*a)->DIlines) 	ListApplyToEachEx ((*a)->DIlines, 1, DisposeDILineChannelList, NULL); ListDispose((*a)->DIlines);  
	if ((*a)->DIports)	ListApplyToEachEx ((*a)->DIports, 1, DisposeDIPortChannelList, NULL); ListDispose((*a)->DIports);  
	if ((*a)->DOlines)	ListApplyToEachEx ((*a)->DOlines, 1, DisposeDOLineChannelList, NULL); ListDispose((*a)->DOlines);  
	if ((*a)->DOports)	ListApplyToEachEx ((*a)->DOports, 1, DisposeDOPortChannelList, NULL); ListDispose((*a)->DOports);  
	if ((*a)->CIchan)	ListApplyToEachEx ((*a)->CIchan, 1, DisposeCIChannelList, NULL); ListDispose((*a)->CIchan);   
	if ((*a)->COchan)	ListApplyToEachEx ((*a)->COchan, 1, DisposeCOChannelList, NULL); ListDispose((*a)->COchan);
	
	// discard supported IO type lists
	if ((*a)->AISupportedMeasTypes)		ListDispose((*a)->AISupportedMeasTypes);
	if ((*a)->AOSupportedOutputTypes)	ListDispose((*a)->AOSupportedOutputTypes);
	if ((*a)->CISupportedMeasTypes)		ListDispose((*a)->CISupportedMeasTypes);
	if ((*a)->COSupportedOutputTypes)	ListDispose((*a)->COSupportedOutputTypes);
	
	OKfree(*a);
}

static DevAttr_type* copy_DevAttr_type(DevAttr_type* attr)
{
	if (!attr) return NULL; 
	DevAttr_type* a = init_DevAttr_type();
	if (!a) return NULL; 

	// product ID
	
	if (!(	a -> name					= StrDup (attr -> name))) 						goto Error; 
	if (!(	a -> type 					= StrDup (attr -> type))) 						goto Error;  
			a -> productNum				= attr -> productNum;
			a -> serial					= attr -> serial;
			
	// min max rates
	
			a -> AISCmaxrate			= attr -> AISCmaxrate;
			a -> AIMCmaxrate			= attr -> AIMCmaxrate;
			a -> AIminrate				= attr -> AIminrate;
			a -> AOmaxrate				= attr -> AOmaxrate;
			a -> AOminrate				= attr -> AOminrate;
			a -> DImaxrate				= attr -> DImaxrate;
			a -> DOmaxrate				= attr -> DOmaxrate;
			
	// physical channel lists 
	
	if (!(	a -> AIchan  				= copy_AIChannelList(attr -> AIchan)))			goto Error;
	if (!(	a -> AOchan  				= copy_AOChannelList(attr -> AOchan)))			goto Error; 
	if (!(	a -> DIlines 				= copy_DILineChannelList(attr -> DIlines)))		goto Error; 	   
	if (!(	a -> DIports 				= copy_DIPortChannelList(attr -> DIports)))		goto Error;   
	if (!(	a -> DOlines 				= copy_DOLineChannelList(attr -> DOlines)))		goto Error; 		  
	if (!(	a -> DOports 				= copy_DOPortChannelList(attr -> DOports)))		goto Error; 		   
	if (!(	a -> CIchan  				= copy_CIChannelList(attr -> CIchan)))			goto Error; 		   
	if (!(	a -> COchan  				= copy_COChannelList(attr -> COchan)))			goto Error; 
	
	// trigger support
	
			a -> AnalogTriggering   	= attr -> AnalogTriggering;
			a -> DigitalTriggering		= attr -> DigitalTriggering;
			
	// trigger usage
	
			a -> AITriggerUsage			= attr -> AITriggerUsage;
			a -> AOTriggerUsage			= attr -> AOTriggerUsage;
			a -> DITriggerUsage			= attr -> DITriggerUsage;
			a -> DOTriggerUsage			= attr -> DOTriggerUsage;
			a -> CITriggerUsage			= attr -> CITriggerUsage;
			a -> COTriggerUsage			= attr -> COTriggerUsage;
			
	// supported IO types
	
	if (!(	a -> AISupportedMeasTypes	= ListCopy(attr->AISupportedMeasTypes)))		goto Error; 
	if (!(	a -> AOSupportedOutputTypes	= ListCopy(attr->AOSupportedOutputTypes)))		goto Error; 
	if (!(	a -> CISupportedMeasTypes	= ListCopy(attr->CISupportedMeasTypes)))		goto Error; 
	
	//null check, return with empty list instead of an error
	if (attr->COSupportedOutputTypes!=NULL)	
		a -> COSupportedOutputTypes	= ListCopy(attr->COSupportedOutputTypes);		
	else  a -> COSupportedOutputTypes=ListCreate(sizeof(int));
	
	return a;
	
Error:
	discard_DevAttr_type(&a);
	return NULL;
}

//------------------------------------------------------------------------------
// AIChannel_type
//------------------------------------------------------------------------------
static AIChannel_type* init_AIChannel_type (void)
{
	AIChannel_type* a = malloc (sizeof(AIChannel_type));
	if (!a) return NULL;
	
	a->physChanName 		= NULL;
	a->supportedMeasTypes   = 0;
	a->Vrngs				= NULL;
	a->Irngs				= NULL;
	a->terminalCfg			= 0;
	a->inUse				= FALSE;
	
	return a;
}

static void discard_AIChannel_type (AIChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedMeasTypes) ListDispose((*a)->supportedMeasTypes);
	discard_IORange_type(&(*a)->Vrngs);
	discard_IORange_type(&(*a)->Irngs);
	OKfree(*a);
}

int CVICALLBACK DisposeAIChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	AIChannel_type** chanPtrPtr = ptrToItem;
	
	discard_AIChannel_type(chanPtrPtr);
	return 0;
}

static AIChannel_type* copy_AIChannel_type (AIChannel_type* channel)
{
	AIChannel_type* a = init_AIChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))			goto Error;
	if (!(	a->supportedMeasTypes		= ListCopy(channel->supportedMeasTypes)))	goto Error;
	if (!(	a->Vrngs					= copy_IORange_type(channel->Vrngs)))		goto Error;  
	if (!(	a->Irngs					= copy_IORange_type(channel->Irngs)))		goto Error;  
			a->terminalCfg				= channel->terminalCfg;
	
	return a;
	
Error:
	discard_AIChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of AIChannel_type* elements
/// HIRET If successful, a list with copied AIChannel_type* elements is returned, otherwise 0.
static ListType	copy_AIChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(AIChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	AIChannel_type* 	chanCopy;
	AIChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_AIChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeAIChannelList, NULL); ListDispose(dest);  
	return 0;
}

static AIChannel_type* GetAIChannel (Dev_type* dev, char physChanName[])
{
	AIChannel_type** 	chPtrPtr;
	size_t				nItems		= ListNumItems(dev->attr->AIchan); 	
	for (size_t i = 1; i <= nItems; i++) { 
		chPtrPtr = ListGetPtrToItem(dev->attr->AIchan, i);
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// AOChannel_type
//------------------------------------------------------------------------------
static AOChannel_type* init_AOChannel_type (void)
{
	AOChannel_type* a = malloc (sizeof(AOChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->supportedOutputTypes		= 0;
	a->Vrngs					= NULL;
	a->Irngs					= NULL;
	a->terminalCfg				= 0;
	a->inUse					= FALSE;
	
	return a;
}

static void discard_AOChannel_type (AOChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedOutputTypes) ListDispose((*a)->supportedOutputTypes);
	discard_IORange_type(&(*a)->Vrngs);
	discard_IORange_type(&(*a)->Irngs);
	OKfree(*a);
}

int CVICALLBACK DisposeAOChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	AOChannel_type** chanPtrPtr = ptrToItem;
	
	discard_AOChannel_type(chanPtrPtr);
	return 0;
}

static AOChannel_type* copy_AOChannel_type (AOChannel_type* channel)
{
	AOChannel_type* a = init_AOChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))				goto Error;
	if (!(	a->supportedOutputTypes		= ListCopy(channel->supportedOutputTypes)))		goto Error;
	if (!(	a->Vrngs					= copy_IORange_type(channel->Vrngs)))			goto Error;  
	if (!(	a->Irngs					= copy_IORange_type(channel->Irngs)))			goto Error;  
			a->terminalCfg				= channel->terminalCfg;
	
	return a;
	
Error:
	discard_AOChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of AOChannel_type* elements
/// HIRET If successful, a list with copied AOChannel_type* elements is returned, otherwise 0.
static ListType	copy_AOChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(AOChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	AOChannel_type* 	chanCopy;
	AOChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_AOChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeAOChannelList, NULL); ListDispose(dest);  
	return 0;
}

static AOChannel_type* GetAOChannel (Dev_type* dev, char physChanName[])
{
	AOChannel_type** 	chPtrPtr;
	size_t				nItems		= ListNumItems(dev->attr->AOchan); 	
	for (size_t i = 1; i <= nItems; i++) {
		chPtrPtr = ListGetPtrToItem(dev->attr->AOchan, i); 
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// DILineChannel_type
//------------------------------------------------------------------------------
static DILineChannel_type* init_DILineChannel_type (void)
{
	DILineChannel_type* a = malloc (sizeof(DILineChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->changeDetectSupported	= 0;
	a->sampClkSupported			= 0;
	a->inUse					= FALSE;
	
	return a;
}

static void discard_DILineChannel_type (DILineChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDILineChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DILineChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DILineChannel_type(chanPtrPtr);
	return 0;
}

static DILineChannel_type* copy_DILineChannel_type (DILineChannel_type* channel)
{
	DILineChannel_type* a = init_DILineChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->changeDetectSupported 	= channel->changeDetectSupported;
			a->sampClkSupported		 	= channel->sampClkSupported;
	
	return a;
	
Error:
	discard_DILineChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DILineChannel_type* elements
/// HIRET If successful, a list with copied DILineChannel_type* elements is returned, otherwise 0.
static ListType	copy_DILineChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DILineChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DILineChannel_type* 	chanCopy;
	DILineChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DILineChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeDILineChannelList, NULL); ListDispose(dest);  
	return 0;
}

static DILineChannel_type* GetDILineChannel (Dev_type* dev, char physChanName[])
{
	DILineChannel_type** 	chPtrPtr;
	size_t					nItems		= ListNumItems(dev->attr->DIlines); 	
	for (size_t i = 1; i <= nItems; i++) {
		chPtrPtr = ListGetPtrToItem(dev->attr->DIlines, i); 	
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------ 
// DIPortChannel_type
//------------------------------------------------------------------------------ 
static DIPortChannel_type* init_DIPortChannel_type (void)
{
	DIPortChannel_type* a = malloc (sizeof(DIPortChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->changeDetectSupported	= 0;
	a->sampClkSupported			= 0;
	a->portWidth				= 0;
	a->inUse					= FALSE;
	
	return a;
}

static void discard_DIPortChannel_type (DIPortChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDIPortChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DIPortChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DIPortChannel_type(chanPtrPtr);
	return 0;
}

static DIPortChannel_type* copy_DIPortChannel_type (DIPortChannel_type* channel)
{
	DIPortChannel_type* a = init_DIPortChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->changeDetectSupported 	= channel->changeDetectSupported;
			a->sampClkSupported		 	= channel->sampClkSupported;
			a->portWidth				= channel->portWidth;
	
	return a;
	
Error:
	discard_DIPortChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DIPortChannel_type* elements
/// HIRET If successful, a list with copied DIPortChannel_type* elements is returned, otherwise 0.
static ListType	copy_DIPortChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DIPortChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DIPortChannel_type* 	chanCopy;
	DIPortChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DIPortChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeDIPortChannelList, NULL); ListDispose(dest);  
	return 0;
}

static DIPortChannel_type* GetDIPortChannel (Dev_type* dev, char physChanName[])
{
	DIPortChannel_type** 	chPtrPtr;
	size_t					nItems		= ListNumItems(dev->attr->DIports); 	
	for (size_t i = 1; i <= nItems; i++) {
		chPtrPtr = ListGetPtrToItem(dev->attr->DIports, i); 		
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// DOLineChannel_type
//------------------------------------------------------------------------------
static DOLineChannel_type* init_DOLineChannel_type (void)
{
	DOLineChannel_type* a = malloc (sizeof(DOLineChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->sampClkSupported			= 0;
	a->inUse					= FALSE;
	
	return a;
}

static void discard_DOLineChannel_type (DOLineChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDOLineChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DOLineChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DOLineChannel_type(chanPtrPtr);
	return 0;
}

static DOLineChannel_type* copy_DOLineChannel_type (DOLineChannel_type* channel)
{
	DOLineChannel_type* a = init_DOLineChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->sampClkSupported		 	= channel->sampClkSupported;
	
	return a;
	
Error:
	discard_DOLineChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DOLineChannel_type* elements
/// HIRET If successful, a list with copied DOLineChannel_type* elements is returned, otherwise 0.
static ListType	copy_DOLineChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DOLineChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DOLineChannel_type* 	chanCopy;
	DOLineChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DOLineChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeDOLineChannelList, NULL); ListDispose(dest);  
	return 0;
}

static DOLineChannel_type* GetDOLineChannel (Dev_type* dev, char physChanName[])
{
	DOLineChannel_type** 	chPtrPtr;
	size_t					nItems		= ListNumItems(dev->attr->DOlines); 	
	for (size_t i = 1; i <= nItems; i++) {	
		chPtrPtr= ListGetPtrToItem(dev->attr->DOlines, i);
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// DOPortChannel_type
//------------------------------------------------------------------------------
static DOPortChannel_type* init_DOPortChannel_type (void)
{
	DOPortChannel_type* a = malloc (sizeof(DOPortChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->sampClkSupported			= 0;
	a->portWidth				= 0;
	a->inUse					= FALSE;
	
	return a;
}

static void discard_DOPortChannel_type (DOPortChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDOPortChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DOPortChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DOPortChannel_type(chanPtrPtr);
	return 0;
}

static DOPortChannel_type* copy_DOPortChannel_type (DOPortChannel_type* channel)
{
	DOPortChannel_type* a = init_DOPortChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->sampClkSupported		 	= channel->sampClkSupported;
			a->portWidth				= channel->portWidth;
	
	return a;
	
Error:
	discard_DOPortChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DOPortChannel_type* elements
/// HIRET If successful, a list with copied DOPortChannel_type* elements is returned, otherwise 0.
static ListType	copy_DOPortChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DOPortChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DOPortChannel_type* 	chanCopy;
	DOPortChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DOPortChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeDOPortChannelList, NULL); ListDispose(dest);  
	return 0;
}

static DOPortChannel_type* GetDOPortChannel (Dev_type* dev, char physChanName[])
{
	DOPortChannel_type** chPtrPtr;
	size_t					nItems		= ListNumItems(dev->attr->DOports); 	
	for (size_t i = 1; i <= nItems; i++) {
		chPtrPtr = ListGetPtrToItem(dev->attr->DOports, i);
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// CIChannel_type
//------------------------------------------------------------------------------
static CIChannel_type* init_CIChannel_type (void)
{
	CIChannel_type* a = malloc (sizeof(CIChannel_type));
	if (!a) return NULL;
	
	a->physChanName 		= NULL;
	a->supportedMeasTypes   = 0;
	a->inUse				= FALSE;
	
	return a;
}

static void discard_CIChannel_type (CIChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedMeasTypes) ListDispose((*a)->supportedMeasTypes);
	
	OKfree(*a);
}

int CVICALLBACK DisposeCIChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	CIChannel_type** chanPtrPtr = ptrToItem;
	
	discard_CIChannel_type(chanPtrPtr);
	return 0;
}

static CIChannel_type* copy_CIChannel_type (CIChannel_type* channel)
{
	CIChannel_type* a = init_CIChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))			goto Error;
	if (!(	a->supportedMeasTypes		= ListCopy(channel->supportedMeasTypes)))	goto Error;
	
	return a;
	
Error:
	discard_CIChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of CIChannel_type* elements
/// HIRET If successful, a list with copied CIChannel_type* elements is returned, otherwise 0.
static ListType	copy_CIChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(CIChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	CIChannel_type* 	chanCopy;
	CIChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_CIChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeCIChannelList, NULL); ListDispose(dest);  
	return 0;
}

static CIChannel_type* GetCIChannel (Dev_type* dev, char physChanName[])
{
	CIChannel_type** 	chPtrPtr;
	size_t				nItems		= ListNumItems(dev->attr->CIchan); 	
	for (size_t i = 1; i <= nItems; i++) {	
		chPtrPtr = ListGetPtrToItem(dev->attr->CIchan, i);
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// COChannel_type
//------------------------------------------------------------------------------
static COChannel_type* init_COChannel_type (void)
{
	COChannel_type* a = malloc (sizeof(COChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->supportedOutputTypes   	= 0;
	a->inUse					= FALSE;
	
	return a;
}

static void discard_COChannel_type (COChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedOutputTypes) ListDispose((*a)->supportedOutputTypes);
	
	OKfree(*a);
}

int CVICALLBACK DisposeCOChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	COChannel_type** chanPtrPtr = ptrToItem;
	
	discard_COChannel_type(chanPtrPtr);
	return 0;
}

static COChannel_type* copy_COChannel_type (COChannel_type* channel)
{
	COChannel_type* a = init_COChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))			goto Error;
	if (!(	a->supportedOutputTypes		= ListCopy(channel->supportedOutputTypes)))	goto Error;
	
	return a;
	
Error:
	discard_COChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of COChannel_type* elements
/// HIRET If successful, a list with copied COChannel_type* elements is returned, otherwise 0.
static ListType	copy_COChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(COChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	COChannel_type* 	chanCopy;
	COChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_COChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
Error:
	ListApplyToEachEx (dest, 1, DisposeCOChannelList, NULL); ListDispose(dest);  
	return 0;
}

static COChannel_type* GetCOChannel (Dev_type* dev, char physChanName[])
{
	COChannel_type** 	chPtrPtr;
	size_t				nItems		= ListNumItems(dev->attr->COchan); 	
	for (size_t i = 1; i <= nItems; i++) {		
		chPtrPtr = ListGetPtrToItem(dev->attr->COchan, i);
		if (!strcmp((*chPtrPtr)->physChanName, physChanName))
			return *chPtrPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// Channel settings
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// ChanSet_type
//------------------------------------------------------------------------------

static void  init_ChanSet_type (ChanSet_type* chanSet, char physChanName[], Channel_type chanType, DiscardChanSetFptr_type discardFptr)
{
	chanSet -> name				= StrDup(physChanName);
	chanSet -> chanType			= chanType;
	chanSet -> chanPanHndl		= 0;
	chanSet -> sinkVChan		= NULL;
	chanSet -> srcVChan			= NULL;
	chanSet -> onDemand			= FALSE;	// hw-timing by default
	chanSet -> discardFptr		= discardFptr;
	chanSet -> startTrig		= NULL;
	chanSet -> referenceTrig	= NULL;
}

static void	discard_ChanSet_type (ChanSet_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->name);
	
	// VChans
	discard_VChan_type((VChan_type**)&(*a)->srcVChan);
	discard_VChan_type((VChan_type**)&(*a)->sinkVChan);
	
	// discard DAQmx task if any
	if ((*a)->taskHndl) {DAQmxClearTask ((*a)->taskHndl); (*a)->taskHndl = 0;}
	
	// discard triggers if any
	discard_TaskTrig_type(&(*a)->startTrig);
	discard_TaskTrig_type(&(*a)->referenceTrig);
	
	OKfree(*a);
}

//------------------------------------------------------------------------------
// ChanSet_AIAO_Voltage_type
//------------------------------------------------------------------------------
static ChanSet_type* init_ChanSet_AIAO_Voltage_type (char physChanName[], Channel_type chanType, AIChannel_type* aiChanAttrPtr, AOChannel_type* aoChanAttrPtr)
{
	ChanSet_AIAO_Voltage_type* a = malloc (sizeof(ChanSet_AIAO_Voltage_type));
	if (!a) return NULL;
	
	// initialize base class
	init_ChanSet_type(&a->baseClass, physChanName, chanType, discard_ChanSet_AIAO_Voltage_type);
	
	// init child
	a -> aiChanAttr	= aiChanAttrPtr;
	a -> aoChanAttr	= aoChanAttrPtr;
	a -> terminal	= Terminal_None;
	a -> Vmin		= 0;
	a -> Vmax		= 0;
	
	return (ChanSet_type*)a;
}

static void	discard_ChanSet_AIAO_Voltage_type (ChanSet_type** a)
{
	if (!*a) return;
	
	// discard base class
	discard_ChanSet_type(a);
}

// AIAO Voltage channel UI callback for setting channel properties
static int ChanSetAIAOVoltage_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_AIAO_Voltage_type* chSetPtr = callbackData;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch(control) {
			
		case AIAOChSet_OnDemand:
			
			GetCtrlVal(panel, control, &chSetPtr->baseClass.onDemand);
			// if this is an On Demand AO channel , adjust VChan such that only Waveform_type* data is allowed
			// otherwise allow both RepeatedWaveform_type* and Waveform_type* data
			if (chSetPtr->baseClass.chanType == Chan_AO_Voltage || chSetPtr->baseClass.chanType == Chan_AO_Current) {
				if (chSetPtr->baseClass.onDemand) {
					DLDataTypes		allowedTypes[] = {DL_Waveform_Double, DL_Float, DL_Double};
					SetSinkVChanDataTypes(chSetPtr->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes); 
				} else {
					DLDataTypes		allowedTypes[] = {DL_Waveform_Double, DL_RepeatedWaveform_Double};
					SetSinkVChanDataTypes(chSetPtr->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes);
				}
			}
				
			break;
			
		case AIAOChSet_Range:
			
			int rangeIdx;
			GetCtrlVal(panel, control, &rangeIdx);
			if (chSetPtr->aiChanAttr) {
				chSetPtr->Vmax = chSetPtr->aiChanAttr->Vrngs->high[rangeIdx];
				chSetPtr->Vmin = chSetPtr->aiChanAttr->Vrngs->low[rangeIdx];
			} else {
				chSetPtr->Vmax = chSetPtr->aoChanAttr->Vrngs->high[rangeIdx];
				chSetPtr->Vmin = chSetPtr->aoChanAttr->Vrngs->low[rangeIdx];
			}
			break;
			
		case AIAOChSet_Terminal:
			
			GetCtrlVal(panel, control, &chSetPtr->terminal);
			break;
			
		case AIAOChSet_VChanName:
			
			// fill out action for VChan
			
			break;
	}
	
	return 0;
}


// CI Edge channel UI callback for setting channel properties
static int ChanSetCIEdge_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	CIEdgeChanSet_type* chSetPtr = callbackData;
	int mode;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch(control) {
			
	//	case CICOChSet_Mode:
			
	//		GetCtrlVal(panel, control,&mode );
		//	chSetPtr->baseClass.
	//		break;
			
	
			
		case SETPAN_VChanName:
			
			// fill out action for VChan
			
			break;
	}
	
	return 0;
}


// CO channel UI callback for setting properties
static int ChanSetCO_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type*	dev = callbackData;
	
	
	if (event != EVENT_COMMIT) return 0;
	
	switch(control) {
			
		case SETPAN_Timeout:
			GetCtrlVal(panel,control,&dev->COTaskSet->timeout);
			break;
		
		case SETPAN_VChanName:
			
			// fill out action for VChan
			
			break;
	}
	
	return 0;
}

//------------------------------------------------------------------------------
// ChanSet_DIDO_type
//------------------------------------------------------------------------------
static ChanSet_type* init_ChanSet_DIDO_type (char physChanName[], Channel_type chanType) 
{
	ChanSet_DIDO_type* a = malloc (sizeof(ChanSet_DIDO_type));
	if (!a) return NULL;
	
	// initialize base class
	init_ChanSet_type(&a->baseClass, physChanName, chanType, discard_ChanSet_DIDO_type);
	
	// init child
	a -> invert		= FALSE;
	
	return (ChanSet_type*)a;
}

static void discard_ChanSet_DIDO_type (ChanSet_type** a)
{
	if (!*a) return; 
	
	// discard base class
	discard_ChanSet_type(a);	
}

//------------------------------------------------------------------------------
// ChanSet_CI_Frequency_type
//------------------------------------------------------------------------------
static ChanSet_type* init_ChanSet_CI_Frequency_type (char physChanName[])
{
	ChanSet_CI_Frequency_type* a = malloc (sizeof(ChanSet_CI_Frequency_type));
	if (!a) return NULL;
	
	// initialize base class
	init_ChanSet_type(&a->baseClass, physChanName, Chan_CI_Frequency, discard_ChanSet_CI_Frequency_type); 
	
			a -> divisor 			= DAQmxDefault_CI_Frequency_Task_Divisor;
			a -> edgeType			= DAQmxDefault_CI_Frequency_Task_Edge;
			a -> freqMax			= DAQmxDefault_CI_Frequency_Task_MaxFreq;
			a -> freqMin			= DAQmxDefault_CI_Frequency_Task_MinFreq;
			a -> measMethod			= DAQmxDefault_CI_Frequency_Task_MeasMethod;
			a -> measTime			= DAQmxDefault_CI_Frequency_Task_MeasTime;
			a -> freqInputTerminal  = NULL;
	if (!(  a -> timing				= init_TaskTiming_type()))	goto Error;
	
	
	return (ChanSet_type*)a;
Error:
	OKfree(a);
	return NULL;
}

static void discard_ChanSet_CI_Frequency_type (ChanSet_type** a)
{
	if (!*a) return; 
	
	ChanSet_CI_Frequency_type* chCIFreqPtr = *(ChanSet_CI_Frequency_type**) a;
	
	OKfree(chCIFreqPtr->freqInputTerminal);
	discard_TaskTiming_type(&chCIFreqPtr->timing);
	
	// discard base class
	discard_ChanSet_type(a);
}


//------------------------------------------------------------------------------
// ChanSet_CO_type
//------------------------------------------------------------------------------
static ChanSet_type* init_ChanSet_CO_type (char physChanName[],Channel_type chanType)
{
	ChanSet_CO_type* a = malloc (sizeof(ChanSet_CO_type));
	if (!a) return NULL;
	
	// initialize base class
	init_ChanSet_type(&a->baseClass, physChanName, chanType, discard_ChanSet_CO_type); 
	
			a ->hightime			= DAQmxDefault_CO_Frequency_Task_hightime;
			a ->lowtime				= DAQmxDefault_CO_Frequency_Task_lowtime;    
			a ->initialdel			= DAQmxDefault_CO_Frequency_Task_initialdel;
			a ->idlestate			= DAQmxDefault_CO_Frequency_Task_idlestate;   
	if (!(  a -> timing				= init_TaskTiming_type()))	goto Error;      
	
	return (ChanSet_type*)a;
Error:
	OKfree(a);
	return NULL;
}

static void discard_ChanSet_CO_type (ChanSet_type** a)
{
	if (!*a) return; 
	
	ChanSet_CO_type* chCOPtr = *(ChanSet_CO_type**) a;
	
//	OKfree(chCOPtr->freqInputTerminal);
//	discard_TaskTiming_type(&chCOPtr->timing);
	
	// discard base class
	discard_ChanSet_type(a);
}



//------------------------------------------------------------------------------
// AIAOTaskSet_type
//------------------------------------------------------------------------------
static AIAOTaskSet_type* init_AIAOTaskSet_type (void)
{
	AIAOTaskSet_type* a = malloc (sizeof(AIAOTaskSet_type));
	if (!a) return NULL;
	
			// init
			a -> chanSet			= 0;
			a -> timing				= NULL;
			
			a -> panHndl			= 0;
			a -> taskHndl			= NULL;
			a -> timeout			= DAQmxDefault_Task_Timeout;
	if (!(	a -> chanSet			= ListCreate(sizeof(ChanSet_type*)))) 	goto Error;
	if (!(	a -> timing				= init_TaskTiming_type()))				goto Error;	
			a -> startTrig			= NULL;
			a -> referenceTrig		= NULL;
			a -> writeAOData		= NULL;
		
	return a;
Error:
	if (a->chanSet) ListDispose(a->chanSet);
	discard_TaskTiming_type(&a->timing);
	OKfree(a);
	return NULL;
}

static void	discard_AIAOTaskSet_type (AIAOTaskSet_type** a)
{
	if (!*a) return;
	
	// DAQmx task
	if ((*a)->taskHndl) {DAQmxClearTask ((*a)->taskHndl); (*a)->taskHndl = 0;}
	// channels
	if ((*a)->chanSet) {
		ChanSet_type** 		chanSetPtrPtr;
		size_t				nItems			= ListNumItems((*a)->chanSet); 	
		for (size_t i = 1; i <= nItems; i++) {			
			chanSetPtrPtr = ListGetPtrToItem((*a)->chanSet, i);
			(*(*chanSetPtrPtr)->discardFptr)	((ChanSet_type**)chanSetPtrPtr);
		}
		ListDispose((*a)->chanSet);
	}
	
	// discard trigger data
	discard_TaskTrig_type(&(*a)->startTrig);
	discard_TaskTrig_type(&(*a)->referenceTrig);
	
	// discard timing info
	discard_TaskTiming_type(&(*a)->timing);
	
	// discard writeAOData structure
	discard_WriteAOData_type(&(*a)->writeAOData);
	
	OKfree(*a);
}

// UI callback to adjust AI task settings
static int AI_Settings_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type*	dev = callbackData;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case Set_SamplingRate:
			
			GetCtrlVal(panel, control, &dev->AITaskSet->timing->sampleRate);   // read in [kHz]
			dev->AITaskSet->timing->sampleRate *= 1000; 					   // transform to [Hz]
			
			break;
			
		case Set_NSamples:
			
			GetCtrlVal(panel, control, &dev->AITaskSet->timing->nSamples);
			// update duration
			//SetCtrlVal(panel, Set_Duration, *dev->AITaskSet->timing->refSampleRate STILL A MESS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			// Decide on how to update various controls based on what is chosen as a reference, such as sampling rate
			break;
			
		case Set_Duration:
			
			double	duration;
			GetCtrlVal(panel, control, &duration);
			
			// calculate number of samples
			dev->AITaskSet->timing->nSamples = (size_t)(*dev->AITaskSet->timing->refSampleRate * duration);
			// update display of number of samples
			SetCtrlVal(panel, Set_NSamples, dev->AITaskSet->timing->nSamples); 
			
			break;
			
		case Set_BlockSize:
			
			GetCtrlVal(panel, control, &dev->AITaskSet->timing->blockSize);
			break;
			
		case Set_UseRefSampRate:
			
			break;
			
		case Set_UseRefNSamples:
			
			break;
			
		case Set_MeasMode:
			
			unsigned int measMode;
			GetCtrlVal(panel, control, &measMode);
			dev->AITaskSet->timing->measMode = measMode;
			break;
	}		
	
	return 0;
}

static int AI_Timing_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type*	dev = callbackData;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case Timing_SampleClkSource:
			
			{	int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(dev->AITaskSet->timing->sampClkSource);
				dev->AITaskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, dev->AITaskSet->timing->sampClkSource);
			}
			break;
			
		case Timing_RefClkSource:
			
			{	int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(dev->AITaskSet->timing->sampClkSource);
				dev->AITaskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, dev->AITaskSet->timing->sampClkSource);
			}
			break;
			
		case Timing_RefClkFreq:
			
			GetCtrlVal(panel, control, &dev->AITaskSet->timing->refClkFreq);
			break;
			
		case Timing_Timeout:
			
			GetCtrlVal(panel, control, &dev->AITaskSet->timeout);
			break;
			
	}
			
	return 0;
}

// UI callback to adjust AO task settings
static int AO_Settings_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type*	dev = callbackData;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case Set_SamplingRate:
			
			GetCtrlVal(panel, control, &dev->AOTaskSet->timing->sampleRate);   // read in [kHz]
			dev->AOTaskSet->timing->sampleRate *= 1000; 					   // transform to [Hz]
			
			break;
			
		case Set_NSamples:
			
			GetCtrlVal(panel, control, &dev->AOTaskSet->timing->nSamples);
			break;
			
		case Set_Duration:
			
			double	duration;
			GetCtrlVal(panel, control, &duration);
			
			// calculate number of samples
			*dev->AOTaskSet->timing->refNSamples = (size_t)(*dev->AOTaskSet->timing->refSampleRate * duration);		// STILL A MESS
			// update display of number of samples
			SetCtrlVal(panel, Set_NSamples, *dev->AOTaskSet->timing->refNSamples); 
			
			break;
			
		case Set_BlockSize:
			
			GetCtrlVal(panel, control, &dev->AOTaskSet->timing->blockSize);
			break;
			
		case Set_UseRefSampRate:
			
			break;
			
		case Set_UseRefNSamples:
			
			break;
			
		case Set_MeasMode:
			
			unsigned int measMode;
			GetCtrlVal(panel, control, &measMode);
			dev->AOTaskSet->timing->measMode = measMode;
			break;
	}
	
	return 0;
}

static int AO_Timing_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type*	dev = callbackData;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) { 
			
		case Timing_SampleClkSource:
			
			{	int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(dev->AOTaskSet->timing->sampClkSource);
				dev->AOTaskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, dev->AOTaskSet->timing->sampClkSource);
			}
			break;
			
		case Timing_RefClkSource:
			
			{	int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(dev->AOTaskSet->timing->sampClkSource);
				dev->AOTaskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, dev->AOTaskSet->timing->sampClkSource);
			}
			break;
			
		case Timing_RefClkFreq:
			
			GetCtrlVal(panel, control, &dev->AOTaskSet->timing->refClkFreq);
			break;
			
		case Timing_Timeout:
			
			GetCtrlVal(panel, control, &dev->AOTaskSet->timeout);
			break;
			
	}
	
	return 0;
	
}

//------------------------------------------------------------------------------
// WriteAOData_type
//------------------------------------------------------------------------------
static WriteAOData_type* init_WriteAOData_type (Dev_type* dev)
{
	ChanSet_type**		chanSetPtrPtr;
	size_t				nAO				= 0;
	size_t				i;
	
	// count number of AO channels using HW-timing
	size_t	nItems	= ListNumItems(dev->AOTaskSet->chanSet); 	
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!(*chanSetPtrPtr)->onDemand) nAO++;
	}
	// return NULL if there are no channels using HW-timing
	if (!nAO) return NULL;
	
	WriteAOData_type* 	a 			= malloc(sizeof(WriteAOData_type));
	if (!a) return NULL;
	
	
	
			a -> writeblock			= dev->AOTaskSet->timing->blockSize;
			a -> numchan			= nAO;
	
	// init
			a -> datain           	= NULL;
			a -> databuff         	= NULL;
			a -> dataout          	= NULL;
			a -> sinkVChans        	= NULL;
			a -> datain_size      	= NULL;
			a -> databuff_size    	= NULL;
			a -> idx              	= NULL;
			a -> datain_repeat    	= NULL;
			a -> datain_remainder 	= NULL;
			a -> datain_loop      	= NULL;
	
	// datain
	if (!(	a -> datain				= malloc(nAO * sizeof(float64*)))) 							goto Error;
	for (i = 0; i < nAO; i++) a->datain[i] = NULL;
	
	// databuff
	if (!(	a -> databuff   		= malloc(nAO * sizeof(float64*)))) 							goto Error;
	for (i = 0; i < nAO; i++) a->databuff[i] = NULL;
	
	// dataout
	if (!(	a -> dataout 			= malloc(nAO * a->writeblock * sizeof(float64))))			goto Error;
	
	// sink VChans
	if (!(	a -> sinkVChans			= malloc(nAO * sizeof(SinkVChan_type*))))					goto Error;
	
	nItems		= ListNumItems(dev->AOTaskSet->chanSet); 
	size_t k 	= 0;
	for (i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!(*chanSetPtrPtr)->onDemand) {
			a->sinkVChans[k] = (*chanSetPtrPtr)->sinkVChan;
			k++;
		}
	}
	
	// datain_size
	if (!(	a -> datain_size 		= malloc(nAO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nAO; i++) a->datain_size[i] = 0;
		
	// databuff_size
	if (!(	a -> databuff_size 		= malloc(nAO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nAO; i++) a->databuff_size[i] = 0;
		
	// idx
	if (!(	a -> idx 				= malloc(nAO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nAO; i++) a->idx[i] = 0;
		
	// datain_repeat
	if (!(	a -> datain_repeat 		= malloc(nAO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nAO; i++) a->datain_repeat[i] = 0;
		
	// datain_remainder
	if (!(	a -> datain_remainder 	= malloc(nAO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nAO; i++) a->datain_remainder[i] = 0;
		
	// datain_loop
	if (!(	a -> datain_loop 		= malloc(nAO * sizeof(BOOL))))								goto Error;
	for (i = 0; i < nAO; i++) a->datain_loop[i] = 0;
	
	return a;
	
Error:
	OKfree(a->datain);
	OKfree(a->databuff);
	OKfree(a->dataout);
	OKfree(a->sinkVChans);
	OKfree(a->datain_size);
	OKfree(a->databuff_size);
	OKfree(a->idx);
	OKfree(a->datain_repeat);
	OKfree(a->datain_remainder);
	OKfree(a->datain_loop);
	
	OKfree(a);
	return NULL;
}

static void	discard_WriteAOData_type (WriteAOData_type** a)
{
	if (!*a) return;
	
	for (size_t i = 0; i < (*a)->numchan; i++) {
		OKfree((*a)->datain[i]);
		OKfree((*a)->databuff[i]);
	}
	
	OKfree((*a)->dataout);
	OKfree((*a)->sinkVChans);
	OKfree((*a)->datain_size);
	OKfree((*a)->databuff_size);
	OKfree((*a)->idx);
	OKfree((*a)->datain_repeat);
	OKfree((*a)->datain_remainder);
	OKfree((*a)->datain_loop);
	
	OKfree(*a);
}

//------------------------------------------------------------------------------
// DIDOTaskSet_type
//------------------------------------------------------------------------------
static DIDOTaskSet_type* init_DIDOTaskSet_type (void)
{
	DIDOTaskSet_type* a = malloc (sizeof(DIDOTaskSet_type));
	if (!a) return NULL;
	
			a -> panHndl			= 0;
			a -> taskHndl			= NULL;
			// init
			a -> chanSet			= 0;
			a -> timing				= NULL;
			
	if (!(	a -> chanSet			= ListCreate(sizeof(ChanSet_type*)))) 	goto Error;
	if (!(	a -> timing				= init_TaskTiming_type()))				goto Error;
			a -> timeout			= DAQmxDefault_Task_Timeout;
			a -> startTrig			= NULL;
			a -> referenceTrig		= NULL;
			
	return a;
Error:
	if (a->chanSet)	ListDispose(a->chanSet);
	discard_TaskTiming_type(&a->timing);
	OKfree(a);
	return NULL;
}

static void	discard_DIDOTaskSet_type (DIDOTaskSet_type** a)
{
	if (!*a) return;
	
	// DAQmx task
	if ((*a)->taskHndl) {DAQmxClearTask ((*a)->taskHndl); (*a)->taskHndl = 0;}
	// channels
	if ((*a)->chanSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems	= ListNumItems((*a)->chanSet);
		for (size_t i = 1; i <= nItems; i++) {	
			chanSetPtrPtr= ListGetPtrToItem((*a)->chanSet, i);
			(*(*chanSetPtrPtr)->discardFptr)	((ChanSet_type**)chanSetPtrPtr);
		}
		ListDispose((*a)->chanSet);
	}
	
	// discard trigger data
	discard_TaskTrig_type(&(*a)->startTrig);
	discard_TaskTrig_type(&(*a)->referenceTrig);
	
	// discard task timing info
	discard_TaskTiming_type(&(*a)->timing);
	
	OKfree(*a); 
}

//------------------------------------------------------------------------------
// CITaskSet_type
//------------------------------------------------------------------------------
static CITaskSet_type* init_CITaskSet_type (void)
{
	CITaskSet_type* a = malloc (sizeof(CITaskSet_type));
	if (!a) return NULL;
	
			a -> timeout		= DAQmxDefault_Task_Timeout;
			// init
			a -> chanTaskSet	= 0;
	
	if (!(	a -> chanTaskSet	= ListCreate(sizeof(ChanSet_type*)))) 	goto Error;
	
	return a;
Error:
	if (a->chanTaskSet) ListDispose(a->chanTaskSet);
	OKfree(a);
	return NULL;
}

static void	discard_CITaskSet_type (CITaskSet_type** a)
{
	if (!*a) return;
	
	// channels and DAQmx tasks
	if ((*a)->chanTaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems			= ListNumItems((*a)->chanTaskSet);
		for (size_t i = 1; i <= nItems; i++) {	
			chanSetPtrPtr = ListGetPtrToItem((*a)->chanTaskSet, i);
			(*(*chanSetPtrPtr)->discardFptr)	((ChanSet_type**)chanSetPtrPtr);
		}
		ListDispose((*a)->chanTaskSet);
	}
	
	OKfree(*a);
}

//------------------------------------------------------------------------------
// COTaskSet_type
//------------------------------------------------------------------------------
static COTaskSet_type* init_COTaskSet_type (void)
{
	COTaskSet_type* a = malloc (sizeof(COTaskSet_type));
	
	if (!a) return NULL;
	
	a -> timeout		= DAQmxDefault_Task_Timeout;
	// init
	a -> chanTaskSet	= 0;
	
	if (!(	a -> chanTaskSet	= ListCreate(sizeof(ChanSet_type*)))) 	goto Error;
	
	return a;
Error:
	if (a->chanTaskSet) ListDispose(a->chanTaskSet);
	OKfree(a);
	return NULL;
}

static void discard_COTaskSet_type (COTaskSet_type** a)
{
	if (!*a) return;
	
		// channels and DAQmx tasks
	if ((*a)->chanTaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems			= ListNumItems((*a)->chanTaskSet);
		for (size_t i = 1; i <= nItems; i++) {	
			chanSetPtrPtr = ListGetPtrToItem((*a)->chanTaskSet, i);
			(*(*chanSetPtrPtr)->discardFptr)	((ChanSet_type**)chanSetPtrPtr);
		}
		ListDispose((*a)->chanTaskSet);
	}
	
	OKfree(*a);
	
}


static int CVICALLBACK CO_Freq_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, control, &selectedChan->frequency);
			break;
	}
	
	return 0;
}

static int CVICALLBACK CO_Duty_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, control, &selectedChan->dutycycle);
			break;
	}
	
	return 0;
}


static int CVICALLBACK CO_InitDelay_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	double delay_in_ms; 
	
	switch (event) {
		case EVENT_COMMIT:
			 GetCtrlVal(panel,control,&delay_in_ms);
			 selectedChan->initialdel=delay_in_ms/1000;   //convert to sec
			break;
	}
	
	return 0;
}




static int CVICALLBACK CO_IdleState_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	int idlestate;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel,control,&idlestate);
			if (idlestate==0) selectedChan->idlestate=DAQmx_Val_Low;
			else selectedChan->idlestate=DAQmx_Val_High;   
			break;
	}
	
	return 0;
}

static int CVICALLBACK CO_THigh_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	double time_in_ms; 
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel,control,&time_in_ms);
			selectedChan->hightime=time_in_ms/1000;   //convert to sec
			break;
	}
	
	return 0;
}

static int CVICALLBACK CO_TLow_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	double time_in_ms;  
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel,control,&time_in_ms);
			selectedChan->lowtime=time_in_ms/1000;   //convert to sec
			break;
	}
	
	return 0;
}

static int CVICALLBACK CO_TicksHigh_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, control, &selectedChan->highticks);
			break;
	}
	
	return 0;
}

static int CVICALLBACK CO_TicksLow_TaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData;
	
	switch (event) {
		case EVENT_COMMIT:
			GetCtrlVal(panel, control, &selectedChan->lowticks);
			break;
	}
	
	return 0;
}



static int CO_Clk_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData; 
	int buffsize = 0; 
	int intval;
	char buf[100];  //temp string hlder to determine string size    
	
	if (event != EVENT_COMMIT) return 0;
	
	
	switch (control) { 
			
		case  CLKPAN_RefClockSlope: 
			  	GetCtrlVal(panel, control,&intval);
				if (intval==TrigSlope_Rising) selectedChan->baseClass.referenceTrig->slope=TrigSlope_Rising;
				if (intval==TrigSlope_Falling) selectedChan->baseClass.referenceTrig->slope=TrigSlope_Falling; 
			break;
 		case  CLKPAN_RefClockType:  
				GetCtrlVal(panel, control,&intval); 
				if (intval==Trig_None) {
					selectedChan->baseClass.referenceTrig->trigType=Trig_None;
					SetCtrlAttribute(panel,CLKPAN_RefClockSlope,ATTR_DIMMED,TRUE);
					SetCtrlAttribute(panel,CLKPAN_RefClkSource,ATTR_DIMMED,TRUE); 
				}
				if (intval==Trig_DigitalEdge) {
					selectedChan->baseClass.referenceTrig->trigType=Trig_DigitalEdge;
					SetCtrlAttribute(panel,CLKPAN_RefClockSlope,ATTR_DIMMED,FALSE);
					SetCtrlAttribute(panel,CLKPAN_RefClkSource,ATTR_DIMMED,FALSE); 
				}
				
			break;
		default:
				OKfree(selectedChan->baseClass.referenceTrig->trigSource);
				GetCtrlVal(panel, control,(char*)&buf);
				buffsize=strlen(buf);
				selectedChan->baseClass.referenceTrig->trigSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, selectedChan->baseClass.referenceTrig->trigSource);
				SetCtrlVal(panel, CLKPAN_RefClkSource, selectedChan->baseClass.referenceTrig->trigSource);  
			break;
			
	
			
	}
	
	return 0;
	
}


static int CO_Trig_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* selectedChan = callbackData; 
	int buffsize = 0; 
	int intval;
	char buf[100];  //temp string hlder to determine string size
	
	if (event != EVENT_COMMIT) return 0;
	
	
	switch (control) { 
			
		case  TRIGPAN_Slope: 
			  	GetCtrlVal(panel, control,&intval);
				if (intval==TrigSlope_Rising) selectedChan->baseClass.startTrig->slope=TrigSlope_Rising;
				if (intval==TrigSlope_Falling) selectedChan->baseClass.startTrig->slope=TrigSlope_Falling; 
			break;
 		case  TRIGPAN_TrigType:  
				GetCtrlVal(panel, control,&intval); 
				if (intval==Trig_None) {
					selectedChan->baseClass.startTrig->trigType=DAQmx_Val_None;
					SetCtrlAttribute(panel,TRIGPAN_Slope,ATTR_DIMMED,TRUE);
					SetCtrlAttribute(panel,TRIGPAN_Source,ATTR_DIMMED,TRUE); 
				}
				if (intval==Trig_DigitalEdge) {
					selectedChan->baseClass.startTrig->trigType=DAQmx_Val_DigEdge;
					SetCtrlAttribute(panel,TRIGPAN_Slope,ATTR_DIMMED,FALSE);
					SetCtrlAttribute(panel,TRIGPAN_Source,ATTR_DIMMED,FALSE); 
				}
				
			break;
		case TRIGPAN_MeasMode:
				GetCtrlVal(panel, control,&intval);
				if (intval==MeasFinite) selectedChan->timing->measMode=MeasFinite;
				if (intval==MeasCont) selectedChan->timing->measMode=MeasCont; 
			break;
		case TRIGPAN_NSamples:
				GetCtrlVal(panel, control,&selectedChan->timing->nSamples);
			break;	
		default:
				OKfree(selectedChan->baseClass.startTrig->trigSource);
				GetCtrlVal(panel, control,(char*)&buf);
				buffsize=strlen(buf);
				selectedChan->baseClass.startTrig->trigSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, selectedChan->baseClass.startTrig->trigSource);
				SetCtrlVal(panel, TRIGPAN_Source, selectedChan->baseClass.startTrig->trigSource);  
			break;
			
	
			
	}
	
	return 0;
	
}

//------------------------------------------------------------------------------ 
// TaskTrig_type
//------------------------------------------------------------------------------ 
static TaskTrig_type* init_TaskTrig_type (double* samplingRate)
{
	TaskTrig_type* a = malloc (sizeof(TaskTrig_type));
	if (!a) return NULL;
	
	a -> trigType				= Trig_None;
	a -> trigSource				= NULL;
	a -> slope					= TrigSlope_Rising;
	a -> level					= 0;
	a -> wndBttm				= 0;
	a -> wndTop					= 0;
	a -> wndTrigCond			= TrigWnd_Entering;
	a -> nPreTrigSamples 		= 2;				// applies only to reference type trigger
	a -> trigSlopeCtrlID		= 0;	
	a -> trigSourceCtrlID		= 0;
	a -> preTrigDurationCtrlID	= 0;
	a -> preTrigNSamplesCtrlID	= 0;
	a -> levelCtrlID			= 0;
	a -> windowTrigCondCtrlID	= 0;
	a -> windowTopCtrlID		= 0;
	a -> windowBottomCtrlID		= 0;
	a -> samplingRate			= samplingRate;
		
	return a;	
}

static void	discard_TaskTrig_type (TaskTrig_type** a)
{
	if (!*a) return;
	OKfree((*a)->trigSource);
	OKfree(*a);
}

//------------------------------------------------------------------------------ 
// TaskTiming_type
//------------------------------------------------------------------------------ 
static TaskTiming_type*	init_TaskTiming_type (void)
{
	TaskTiming_type* a = malloc(sizeof(TaskTiming_type));
	if (!a) return NULL;
	
	a -> measMode				= DAQmxDefault_Task_MeasMode;
	a -> sampleRate				= DAQmxDefault_Task_SampleRate;
	a -> nSamples				= DAQmxDefault_Task_NSamples;
	a -> blockSize				= DAQmxDefault_Task_BlockSize;
	a -> refNSamples			= &a->nSamples;
	a -> refSampleRate			= &a->sampleRate;
	a -> sampClkSource			= NULL;   								// use onboard clock for sampling
	a -> sampClkEdge			= SampClockEdge_Rising;
	a -> refClkSource			= NULL;									// onboard clock has no external reference to sync to
	a -> refClkFreq				= DAQmxDefault_Task_RefClkFreq;
	a -> useRefNSamples			= FALSE;
	a -> useRefSamplingRate 	= FALSE;
	
	return a;
}

static void discard_TaskTiming_type	(TaskTiming_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->sampClkSource);
	OKfree((*a)->refClkSource);
	
	OKfree(*a);
}

//------------------------------------------------------------------------------ 
static IORange_type* GetIORanges (char devName[], int rangeType)
{
	IORange_type* 	IORngsPtr	= init_IORange_type();
	int 			nelem		= 0; 
	double* 		rngs     	= NULL;
	
	if (!IORngsPtr) return NULL;
	
	if((nelem = DAQmxGetDeviceAttribute(devName, rangeType, NULL)) < 0) goto Error;
	if (!nelem) return IORngsPtr;  // no ranges available
	
	IORngsPtr->Nrngs = nelem/2;
	rngs = malloc (nelem * sizeof(double));
	if (!rngs) goto Error; 
	
	IORngsPtr->low = malloc((IORngsPtr->Nrngs) * sizeof(double));
	if (!IORngsPtr->low) goto Error;
	
	IORngsPtr->high = malloc((IORngsPtr->Nrngs) * sizeof(double));
	if (!IORngsPtr->high) goto Error;
	
	if (DAQmxGetDeviceAttribute(devName, rangeType, rngs, nelem) < 0) goto Error;
	
	for (size_t i = 0; i < IORngsPtr->Nrngs; i++){
		IORngsPtr->low[i] = rngs[2*i];	
		IORngsPtr->high[i] = rngs[2*i+1];
	}
	
	OKfree(rngs);
	
	return IORngsPtr;
	
Error:
	OKfree(rngs);
	discard_IORange_type(&IORngsPtr);
	return NULL;
}

static void VChanConnected (VChan_type* self, VChan_type* connectedVChan)
{
	// implement how to handle VChan connect/disconnect by this module
}
static void VChanDisconnected (VChan_type* self, VChan_type* disconnectedVChan)
{
	// implement how to handle VChan connect/disconnect by this module
}

static ListType GetSupportedIOTypes (char devName[], int IOType)
{
	ListType 	IOTypes 	= ListCreate(sizeof(int));
	int			nelem		= 0;
	int*		io			= NULL;
	
	if (!IOTypes) return 0;
	
	if((nelem = DAQmxGetDeviceAttribute(devName, IOType, NULL)) < 0) goto Error;
	
	io = malloc (nelem * sizeof(int));
	if (!io) goto Error; // also if nelem = 0, i.e. no IO types found
	
	if (DAQmxGetDeviceAttribute(devName, IOType, io, nelem) < 0) goto Error;
	
	for (size_t i = 0; i < nelem; i++)
		ListInsertItem(IOTypes, &io[i], END_OF_LIST);
	
	return IOTypes;
	
Error:
	OKfree(io);
	ListDispose(IOTypes);
	return 0;
}

/// HIFN Obtains a list of physical channel properties for a given DAQmx channel attribute that returns an array of properties.
/// HIPAR chanName/ Physical channel name, e.g. dev1/ai0
/// HIPAR chanIOAttribute/ Physical channel property returning an array such as: DAQmx_PhysicalChan_AI_SupportedMeasTypes, DAQmx_PhysicalChan_AO_SupportedOutputTypes,etc.
static ListType	GetPhysChanPropertyList	(char chanName[], int chanProperty)
{
	ListType 	propertyList 	= ListCreate(sizeof(int));
	int			nelem			= 0;
	int*		properties		= NULL;
	
	if (!propertyList) return 0;
	
	nelem = DAQmxGetPhysicalChanAttribute(chanName, chanProperty, NULL); 
	if (!nelem) return propertyList;
	if (nelem < 0) goto Error;
	
	properties = malloc (nelem * sizeof(int));
	if (!properties) goto Error; // also if nelem = 0, i.e. no properties found
	
	if (DAQmxGetPhysicalChanAttribute(chanName, chanProperty, properties, nelem) < 0) goto Error;
	
	for (size_t i = 0; i < nelem; i++)
		ListInsertItem(propertyList, &properties[i], END_OF_LIST);
	
	return propertyList;
	
Error:
	OKfree(properties);
	ListDispose(propertyList);
	return 0;
}

static void	PopulateChannels (Dev_type* dev)
{
	int							ioVal;
	int							ioMode;
	int							ioType;
	char*						shortChName;
	AIChannel_type**			AIChanPtrPtr;
	AOChannel_type**   			AOChanPtrPtr;
	DILineChannel_type**		DILineChanPtrPtr;
	DIPortChannel_type**  		DIPortChanPtrPtr;
	DOLineChannel_type**		DOLineChanPtrPtr;
	DOPortChannel_type**  		DOPortChanPtrPtr;
	CIChannel_type**			CIChanPtrPtr;
	COChannel_type**   			COChanPtrPtr;
	size_t						n;
	
	GetCtrlVal(dev->devPanHndl, TaskSetPan_IO, &ioVal);
	GetCtrlVal(dev->devPanHndl, TaskSetPan_IOMode, &ioMode);
	GetCtrlVal(dev->devPanHndl, TaskSetPan_IOType, &ioType);
	
	ClearListCtrl(dev->devPanHndl, TaskSetPan_PhysChan); 
	
	switch (ioVal) {
			
		case DAQmxAcquire:
			
			switch (ioMode) {
				
				case DAQmxAnalog:
					
					n = ListNumItems(dev->attr->AIchan);
					for (int i = 1; i <= n; i++) {
						AIChanPtrPtr = ListGetPtrToItem(dev->attr->AIchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*AIChanPtrPtr)->supportedMeasTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*AIChanPtrPtr)->inUse) {
							shortChName = strstr((*AIChanPtrPtr)->physChanName, "/") + 1;  
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*AIChanPtrPtr)->physChanName);
						}
					}
					
					break;
					
				case DAQmxDigital:
					
					switch (ioType) {
						
						case DAQmxDigLines:
							
							n = ListNumItems(dev->attr->DIlines);
							for (int i = 1; i <= n; i++) {
								DILineChanPtrPtr = ListGetPtrToItem(dev->attr->DIlines, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DILineChanPtrPtr)->inUse) {
									shortChName = strstr((*DILineChanPtrPtr)->physChanName, "/") + 1;  
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DILineChanPtrPtr)->physChanName);
								}
							}
							
							break;
							
						case DAQmxDigPorts:
							
							n = ListNumItems(dev->attr->DIports);
							for (int i = 1; i <= n; i++) {
								DIPortChanPtrPtr = ListGetPtrToItem(dev->attr->DIports, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DIPortChanPtrPtr)->inUse) {
									shortChName = strstr((*DIPortChanPtrPtr)->physChanName, "/") + 1;  
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DIPortChanPtrPtr)->physChanName);
								}
							}
							
							break;
					}
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->CIchan);
					for (int i = 1; i <= n; i++) {
						CIChanPtrPtr = ListGetPtrToItem(dev->attr->CIchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*CIChanPtrPtr)->supportedMeasTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*CIChanPtrPtr)->inUse) {
							shortChName = strstr((*CIChanPtrPtr)->physChanName, "/") + 1; 
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*CIChanPtrPtr)->physChanName);
						}
					}
					
					break;
			}
			
			break;
			
		case DAQmxGenerate:
			
			switch (ioMode) {
				
				case DAQmxAnalog:
					
					n = ListNumItems(dev->attr->AOchan);
					for (int i = 1; i <= n; i++) {
						AOChanPtrPtr = ListGetPtrToItem(dev->attr->AOchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*AOChanPtrPtr)->supportedOutputTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*AOChanPtrPtr)->inUse) {
							shortChName = strstr((*AOChanPtrPtr)->physChanName, "/") + 1;
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*AOChanPtrPtr)->physChanName);
						}
					}
					
					break;
					
				case DAQmxDigital:
					
						switch (ioType) {
						
						case DAQmxDigLines:
							
							n = ListNumItems(dev->attr->DOlines);
							for (int i = 1; i <= n; i++) {
								DOLineChanPtrPtr = ListGetPtrToItem(dev->attr->DOlines, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DOLineChanPtrPtr)->inUse) {
									shortChName = strstr((*DOLineChanPtrPtr)->physChanName, "/") + 1;
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DOLineChanPtrPtr)->physChanName);
								}
							}
							
							break;
							
						case DAQmxDigPorts:
							
							n = ListNumItems(dev->attr->DOports);
							for (int i = 1; i <= n; i++) {
								DOPortChanPtrPtr = ListGetPtrToItem(dev->attr->DOports, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DOPortChanPtrPtr)->inUse) {
									shortChName = strstr((*DOPortChanPtrPtr)->physChanName, "/") + 1;
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DOPortChanPtrPtr)->physChanName);
								}
							}
							
							break;
					}
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->COchan);
					for (int i = 1; i <= n; i++) {
						COChanPtrPtr = ListGetPtrToItem(dev->attr->COchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*COChanPtrPtr)->supportedOutputTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*COChanPtrPtr)->inUse) {
							shortChName = strstr((*COChanPtrPtr)->physChanName, "/") + 1;
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*COChanPtrPtr)->physChanName);
						}
					}
					
					break;
			}
			
			break;
	}
}

//------------------------------------------------------------------------------
// IORange_type
//------------------------------------------------------------------------------

static IORange_type* init_IORange_type(void)
{
	IORange_type* a = malloc (sizeof(IORange_type));
	if (!a) return NULL;
	
	a -> Nrngs 	= 0;
	a -> low 	= NULL;
	a -> high 	= NULL;
	
	return a;
}

static void discard_IORange_type(IORange_type** a)
{
	if (!*a) return;
	
	(*a)->Nrngs = 0;
	OKfree((*a)->low);
	OKfree((*a)->high);
	OKfree(*a);
}

static IORange_type* copy_IORange_type (IORange_type* IOrange)
{
	IORange_type* a = init_IORange_type();
	if (!a) return NULL;
	
	if (!IOrange->Nrngs) return a; // no ranges
	// allocate memory for the ranges
	a->low = malloc(IOrange->Nrngs * sizeof(double));
	if (!a->low) goto Error;
	a->high = malloc(IOrange->Nrngs * sizeof(double));
	if (!a->high) goto Error;
	
	a->Nrngs = IOrange->Nrngs;
	
	// copy ranges
	memcpy(a->low, IOrange->low, IOrange->Nrngs * sizeof(double));  
	memcpy(a->high, IOrange->high, IOrange->Nrngs * sizeof(double));
	
	return a;
	
Error:
	discard_IORange_type(&a);
	return NULL;
}

//------------------------------------------------------------------------------
// Dev_type
//------------------------------------------------------------------------------

static Dev_type* init_Dev_type (NIDAQmxManager_type* niDAQModule, DevAttr_type** attr, char taskControllerName[])
{
	Dev_type* dev = malloc (sizeof(Dev_type));
	if (!dev) return NULL;
	
	dev -> niDAQModule			= niDAQModule;
	dev -> devPanHndl			= 0;
	dev -> attr					= *attr;
	
	//-------------------------------------------------------------------------------------------------
	// Task Controller
	//-------------------------------------------------------------------------------------------------
	dev -> taskController = init_TaskControl_type (taskControllerName, dev, ConfigureTC, IterateTC, AbortIterationTC, StartTC, 
						  ResetTC, DoneTC, StoppedTC, DimTC, NULL, ModuleEventHandler, ErrorTC);
	if (!dev->taskController) {discard_DevAttr_type(attr); free(dev); return NULL;}
	
	//--------------------------
	// DAQmx task settings
	//--------------------------
	
	dev -> AITaskSet			= NULL;			
	dev -> AOTaskSet			= NULL;
	dev -> DITaskSet			= NULL;
	dev -> DOTaskSet			= NULL;
	dev -> CITaskSet			= NULL;
	dev -> COTaskSet			= NULL;	
	
	return dev;
}

static void discard_Dev_type(Dev_type** a)
{
	if (!(*a)) return;
	
	// device properties
	discard_DevAttr_type(&(*a)->attr);
	
	// task controller for this device
	discard_TaskControl_type(&(*a)->taskController);
	
	// DAQmx task settings
	discard_AIAOTaskSet_type(&(*a)->AITaskSet);
	discard_AIAOTaskSet_type(&(*a)->AOTaskSet);
	discard_DIDOTaskSet_type(&(*a)->DITaskSet);
	discard_DIDOTaskSet_type(&(*a)->DOTaskSet);
	discard_CITaskSet_type(&(*a)->CITaskSet);
	discard_COTaskSet_type(&(*a)->COTaskSet);
	
	OKfree(*a);
}

//===============================================================================================================
//									NI DAQ module loading and device management
//===============================================================================================================

int	Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	int						error						= 0;
	NIDAQmxManager_type* 	nidaq						= (NIDAQmxManager_type*) mod;  
	int						menuItemDevicesHndl			= 0;
	
	// main panel 
	errChk(nidaq->mainPanHndl = LoadPanel(workspacePanHndl, MOD_NIDAQmxManager_UI, NIDAQmxPan));
	// device list panel
	errChk(nidaq->devListPanHndl = LoadPanel(workspacePanHndl, MOD_NIDAQmxManager_UI, DevListPan));
	// DAQmx task settings panel
	errChk(nidaq->taskSetPanHndl = LoadPanel(workspacePanHndl, MOD_NIDAQmxManager_UI, TaskSetPan));
	   
	// connect module data and user interface callbackFn to all direct controls in the main panel
	SetCtrlsInPanCBInfo(mod, MainPan_CB, nidaq->mainPanHndl);
	// add callback data to the Add button from the device list panel
	SetCtrlAttribute(nidaq->devListPanHndl, DevListPan_AddBTTN, ATTR_CALLBACK_DATA, nidaq); 
	
	// add menubar and link it to module data
	nidaq->menuBarHndl 		= NewMenuBar(nidaq->mainPanHndl);
	menuItemDevicesHndl 	= NewMenu(nidaq->menuBarHndl, "Devices", -1);
	
	NewMenuItem(nidaq->menuBarHndl, menuItemDevicesHndl, "Add", -1, (VAL_MENUKEY_MODIFIER | 'A'), ManageDevPan_CB, mod); 
	nidaq->deleteDevMenuItemID = NewMenuItem(nidaq->menuBarHndl, menuItemDevicesHndl, "Delete", -1, (VAL_MENUKEY_MODIFIER | 'D'), DeleteDev_CB, mod);
	// dim "Delete" menu item since there are no devices to be deleted
	SetMenuBarAttribute(nidaq->menuBarHndl, nidaq->deleteDevMenuItemID, ATTR_DIMMED, 1); 
	
	return 0;
Error:
	
	if (nidaq->mainPanHndl) {DiscardPanel(nidaq->mainPanHndl); nidaq->mainPanHndl = 0;}
	if (nidaq->devListPanHndl) {DiscardPanel(nidaq->devListPanHndl); nidaq->devListPanHndl = 0;}
	if (nidaq->taskSetPanHndl) {DiscardPanel(nidaq->taskSetPanHndl); nidaq->taskSetPanHndl = 0;} 
	
	return error;
}

/// HIFN Populates table and a devlist of DevAttr_type with NIDAQmxManager devices and their properties.
/// HIRET positive value for the number of devices found, 0 if there are no devices and negative values for error.
static int init_DevList (ListType devlist, int panHndl, int tableCtrl)
{
	int							error				= 0;
    int 						buffersize  		= 0;
	char* 						tmpnames			= NULL;
	char* 						tmpsubstr			= NULL;
	char* 						dev_pt     			= NULL;
	char** 						idxnames  			= NULL;          // Used to break up the names string
	char** 						idxstr    			= NULL;          // Used to break up the other strings like AI channels
	DevAttr_type* 				devAttrPtr; 
	AIChannel_type*				newAIChanPtr		= NULL;
	AOChannel_type*				newAOChanPtr		= NULL;
	DILineChannel_type*			newDILineChanPtr	= NULL;
	DIPortChannel_type*			newDIPortChanPtr	= NULL;
	DOLineChannel_type*			newDOLineChanPtr	= NULL;
	DOPortChannel_type*			newDOPortChanPtr	= NULL;
	CIChannel_type*				newCIChanPtr		= NULL;
	COChannel_type*				newCOChanPtr		= NULL;
	int 						ndev        		= 0;
	int 						columns     		= 0;
	unsigned int   				nAI;          						// # AI chan
	unsigned int   				nAO;          						// # AO chan
	unsigned int   				nDIlines;     						// # DI lines
	unsigned int   				nDIports;     						// # DI ports
	unsigned int   				nDOlines;     						// # DO lines
	unsigned int   				nDOports;     						// # DO ports
	unsigned int   				nCI;          						// # Counter inputs
	unsigned int   				nCO;          						// # Counter outputs
								
	// Allocate memory for pointers to work on the strings
	nullChk(idxnames = malloc(sizeof(char*)));
	nullChk(idxstr = malloc(sizeof(char*)));
	*idxstr = NULL;
	
	// Get number of table columns
	GetNumTableColumns (panHndl, DevListPan_DAQTable, &columns);
	
	// Empty list of devices detected in the computer
	empty_DevList(devlist);
	
	// Empty Table contents
	DeleteTableRows (panHndl, DevListPan_DAQTable, 1, -1);
	
	// Get string of device names in the computer
	buffersize = DAQmxGetSystemInfoAttribute (DAQmx_Sys_DevNames, NULL);  
	if (!buffersize) {
		SetCtrlAttribute (panHndl, DevListPan_DAQTable, ATTR_LABEL_TEXT,"No devices found"); 
		return 0;
	}
	
	nullChk(tmpnames = malloc (buffersize));
	DAQmxGetSystemInfoAttribute (DAQmx_Sys_DevNames, tmpnames, buffersize);
	
	// Get first dev name using malloc, dev_pt is dynamically allocated
	*idxnames = tmpnames;
	dev_pt = substr (", ", idxnames);
	
	/* Populate table and device structure with device info */
	while (dev_pt!=NULL)
	{
		ndev++; 
		// Init DevAttr_type dev structure and fill it with data
		
		if (!(devAttrPtr = init_DevAttr_type())) {   
			DLMsg("Error: DevAttr_type structure could not be initialized.\n\n", 1);
			return -1;
		}
		
		// 1. Name (dynamically allocated)
		devAttrPtr->name = dev_pt;
		
		// 2. Type (dynamically allocated)
		errChk(buffersize = DAQmxGetDeviceAttribute(dev_pt, DAQmx_Dev_ProductType, NULL));
		nullChk(devAttrPtr->type = malloc(buffersize));
		
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_ProductType, devAttrPtr->type, buffersize));
		
		// 3. Product Number
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_ProductNum, &devAttrPtr->productNum));
		
		// 4. S/N
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_SerialNum, &devAttrPtr->serial)); 
		
		//------------------------------------------------									  
		// Channel properties							
		//------------------------------------------------
		
		// 5. AI
		nAI = 0;
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_PhysicalChans, NULL));
		if (buffersize){   //process info if available         
			nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_PhysicalChans, *idxstr, buffersize));
		 															
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newAIChanPtr = init_AIChannel_type())) goto Error;			
				newAIChanPtr->physChanName 			= tmpsubstr;
				newAIChanPtr->supportedMeasTypes 	= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_AI_SupportedMeasTypes);  
				newAIChanPtr->Vrngs					= GetIORanges(dev_pt, DAQmx_Dev_AI_VoltageRngs);
				newAIChanPtr->Irngs					= GetIORanges(dev_pt, DAQmx_Dev_AI_CurrentRngs);
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_AI_TermCfgs, &newAIChanPtr->terminalCfg); 
				ListInsertItem (devAttrPtr->AIchan, &newAIChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nAI++; 															
			} 	
		}
		
		// 6. AO
		nAO = 0;
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_PhysicalChans, NULL));  
		if (buffersize){   //process info if available     
			nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_PhysicalChans, *idxstr, buffersize));
																
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newAOChanPtr = init_AOChannel_type())) goto Error;			
				newAOChanPtr->physChanName 			= tmpsubstr;
				newAOChanPtr->supportedOutputTypes 	= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_AO_SupportedOutputTypes);  
				newAOChanPtr->Vrngs					= GetIORanges(dev_pt, DAQmx_Dev_AO_VoltageRngs);
				newAOChanPtr->Irngs					= GetIORanges(dev_pt, DAQmx_Dev_AO_CurrentRngs);
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_AO_TermCfgs, &newAOChanPtr->terminalCfg); 
				ListInsertItem (devAttrPtr->AOchan, &newAOChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nAO++; 															
			}
		}
					 
		// 7. DI lines
		nDIlines = 0; 
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_Lines, NULL));  
		if (buffersize){   //process info if available           
			nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_Lines, *idxstr, buffersize));
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newDILineChanPtr = init_DILineChannel_type())) goto Error;			
				newDILineChanPtr->physChanName 			= tmpsubstr;
				newDILineChanPtr->sampModes				= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_DI_SampModes);  
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DI_ChangeDetectSupported, &newDILineChanPtr->changeDetectSupported);
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DI_SampClkSupported, &newDILineChanPtr->sampClkSupported);
				ListInsertItem (devAttrPtr->DIlines, &newDILineChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nDIlines++; 															
			} 	
		}
		
		// 8. DI ports
		nDIports = 0; 
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_Ports, NULL));
		if (buffersize){   //process info if available   
			nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_Ports, *idxstr, buffersize));
																	
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newDIPortChanPtr = init_DIPortChannel_type())) goto Error;			
				newDIPortChanPtr->physChanName 			= tmpsubstr;
				newDIPortChanPtr->sampModes				= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_DI_SampModes);  
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DI_ChangeDetectSupported, &newDIPortChanPtr->changeDetectSupported);
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DI_SampClkSupported, &newDIPortChanPtr->sampClkSupported);
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DI_PortWidth, &newDIPortChanPtr->portWidth);
				ListInsertItem (devAttrPtr->DIports, &newDIPortChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nDIports++; 															
			} 
		}
		
		// 9. DO lines
		nDOlines = 0; 
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_Lines, NULL));  
		if (buffersize){   //process info if available   
			nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_Lines, *idxstr, buffersize));
																	
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newDOLineChanPtr = init_DOLineChannel_type())) goto Error;			
				newDOLineChanPtr->physChanName 			= tmpsubstr;
				newDOLineChanPtr->sampModes				= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_DO_SampModes);  
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DO_SampClkSupported, &newDOLineChanPtr->sampClkSupported);
				ListInsertItem (devAttrPtr->DOlines, &newDOLineChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nDOlines++; 															
			} 
		}
		
		// 10. DO ports
		nDOports = 0; 
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_Ports, NULL));
		if (buffersize){   //process info if available    
			nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_Ports, *idxstr, buffersize));
																	
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newDOPortChanPtr = init_DOPortChannel_type())) goto Error;			
				newDOPortChanPtr->physChanName 			= tmpsubstr;
				newDOPortChanPtr->sampModes				= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_DO_SampModes);  
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DO_SampClkSupported, &newDOPortChanPtr->sampClkSupported);
				DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_DO_PortWidth, &newDOPortChanPtr->portWidth);  
				ListInsertItem (devAttrPtr->DOports, &newDOPortChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nDOports++; 															
			}
		}
		
		// 11. CI 
		nCI = 0;
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_CI_PhysicalChans, NULL));
		if (buffersize){   //process info if available        
		nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_CI_PhysicalChans, *idxstr, buffersize));
																	
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newCIChanPtr = init_CIChannel_type())) goto Error;			
				newCIChanPtr->physChanName 			= tmpsubstr;
				newCIChanPtr->supportedMeasTypes 	= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_CI_SupportedMeasTypes);  
				ListInsertItem (devAttrPtr->CIchan, &newCIChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nCI++; 															
			} 
		}
		
		// 12. CO 
		nCO = 0; 
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_CO_PhysicalChans, NULL));
		if (buffersize){   //process info if available
			nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
			errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_CO_PhysicalChans, *idxstr, buffersize));
																	
			tmpsubstr = substr (", ", idxstr);										
			while (tmpsubstr != NULL) {												
				if (!(newCOChanPtr = init_COChannel_type())) goto Error;			
				newCOChanPtr->physChanName 			= tmpsubstr;
				newCOChanPtr->supportedOutputTypes 	= GetPhysChanPropertyList(tmpsubstr, DAQmx_PhysicalChan_CO_SupportedOutputTypes);  
				ListInsertItem (devAttrPtr->COchan, &newCOChanPtr, END_OF_LIST);						
				tmpsubstr = substr (", ", idxstr); 									
				nCO++; 															
			} 
		}
		
		// 13. Single Channel AI max rate
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MaxSingleChanRate, &devAttrPtr->AISCmaxrate)); 	// [Hz]
		// 14. Multiple Channel AI max rate
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MaxMultiChanRate, &devAttrPtr->AIMCmaxrate));  	// [Hz]
		// 15. AI min rate 
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MinRate, &devAttrPtr->AIminrate));  	         	// [Hz] 
		// 16. AO max rate  
//test		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MaxRate, &devAttrPtr->AOmaxrate)); 			 	// [Hz]
		// 17. AO min rate  
//test		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MinRate, &devAttrPtr->AOminrate)); 			 	// [Hz]
		// 18. DI max rate
//test		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_MaxRate, &devAttrPtr->DImaxrate)); 			 	// [Hz]
		// 19. DO max rate
//test		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_MaxRate, &devAttrPtr->DOmaxrate));    		 		// [Hz]
		
		// --- Triggering ---
		
		// Analog triggering supported ?
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AnlgTrigSupported, &devAttrPtr->AnalogTriggering));
		// Digital triggering supported ?
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DigTrigSupported, &devAttrPtr->DigitalTriggering));
		// Get types of AI trigger usage
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_TrigUsage, &devAttrPtr->AITriggerUsage));
		// Get types of AO trigger usage
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_TrigUsage, &devAttrPtr->AOTriggerUsage));
		// Get types of DI trigger usage
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_TrigUsage, &devAttrPtr->DITriggerUsage));
		// Get types of DO trigger usage
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_TrigUsage, &devAttrPtr->DOTriggerUsage));
		// Get types of CI trigger usage
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_CI_TrigUsage, &devAttrPtr->CITriggerUsage));
		// Get types of CO trigger usage
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_CO_TrigUsage, &devAttrPtr->COTriggerUsage));
		
		// --- Supported IO Types ---
		
		devAttrPtr->AISupportedMeasTypes	= GetSupportedIOTypes(dev_pt, DAQmx_Dev_AI_SupportedMeasTypes);
		devAttrPtr->AOSupportedOutputTypes	= GetSupportedIOTypes(dev_pt, DAQmx_Dev_AO_SupportedOutputTypes);
		devAttrPtr->CISupportedMeasTypes	= GetSupportedIOTypes(dev_pt, DAQmx_Dev_CI_SupportedMeasTypes);
		devAttrPtr->COSupportedOutputTypes	= GetSupportedIOTypes(dev_pt, DAQmx_Dev_CO_SupportedOutputTypes);
		
		/*  Set DAQ table cell properties */

		// Justify center left
		SetTableColumnAttribute (panHndl, tableCtrl, -1, ATTR_CELL_JUSTIFY, VAL_CENTER_CENTER_JUSTIFIED);
		// Insert the new row
    	InsertTableRows (panHndl, tableCtrl, -1, 1, VAL_USE_MASTER_CELL_TYPE);
		// Highlight first row
		if (ndev == 1) SetTableCellRangeAttribute (panHndl, tableCtrl, MakeRect(1,1,1,columns),ATTR_TEXT_BGCOLOR, VAL_YELLOW); 
		SetTableCellRangeAttribute (panHndl, tableCtrl, MakeRect(1,1,ndev,columns), ATTR_CELL_MODE , VAL_INDICATOR);
		// Set currently selected device from table
		currDev=1;
		
		// Macro to write DAQ Device data to table
		#define DAQTable(cell, info) SetTableCellVal (panHndl, tableCtrl , MakePoint (cell,ndev), info)     
        
		// 1. Name
		DAQTable(1, devAttrPtr->name);
		// 2. Type
		DAQTable(2, devAttrPtr->type); 
		// 3. Product Number
		DAQTable(3, devAttrPtr->productNum); 
		// 4. S/N
		DAQTable(4, devAttrPtr->serial);
		// 5. # AI chan 
		DAQTable(5, nAI);
		// 6. # AO chan
		DAQTable(6, nAO);
		// 7. # DI lines
		DAQTable(7, nDIlines);
		// 8. # DI ports  
		DAQTable(8, nDIports);
		// 9. # DO lines
		DAQTable(9, nDOlines);
		// 10. # DO ports  
		DAQTable(10, nDOports);
		// 11. # CI 
		DAQTable(11, nCI);
		// 12. # CO ports  
		DAQTable(12, nCO);
		// 13. Single Channel AI max rate  
		DAQTable(13, devAttrPtr->AISCmaxrate/1000); // display in [kHz]
		// 14. Multiple Channel AI max rate 
		DAQTable(14, devAttrPtr->AIMCmaxrate/1000); // display in [kHz] 
		// 15. AI min rate
		DAQTable(15, devAttrPtr->AIminrate/1000);   // display in [kHz] 
		// 16. AO max rate
		DAQTable(16, devAttrPtr->AOmaxrate/1000);   // display in [kHz]
		// 17. AO min rate  
		DAQTable(17, devAttrPtr->AOminrate/1000);   // display in [kHz] 
		// 18. DI max rate
		DAQTable(18, devAttrPtr->DImaxrate/1000);   // display in [kHz] 
		// 19. DO max rate  
		DAQTable(19, devAttrPtr->DOmaxrate/1000);   // display in [kHz]   
		
		
		/* Add device to list */
		ListInsertItem(devlist, &devAttrPtr, END_OF_LIST);
		
		//Get the next device name in the list
		dev_pt = substr (", ", idxnames); 
	}
	
	SetCtrlAttribute (panHndl, tableCtrl, ATTR_LABEL_TEXT, "Devices found");  
	
	OKfree(*idxnames);
	OKfree(*idxstr);
	OKfree(idxnames);
	OKfree(idxstr);
	OKfree(tmpnames);
	
	return ndev;
	
Error:
	
	OKfree(*idxnames);
	OKfree(*idxstr);
	OKfree(idxnames);
	OKfree(idxstr);
	OKfree(tmpnames);
	discard_DevAttr_type(&devAttrPtr);
	return error;
}

static void empty_DevList (ListType devList)
{
    DevAttr_type** devPtrPtr;
	
	while (ListNumItems (devList)) {
		devPtrPtr = ListGetPtrToItem (devList, END_OF_LIST);
		discard_DevAttr_type (devPtrPtr);
		ListRemoveItem (devList, NULL, END_OF_LIST);
	}
}

static int CVICALLBACK 	MainPan_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	return 0;
}

static void CVICALLBACK ManageDevPan_CB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle)
{
	NIDAQmxManager_type* 	nidaq 		= (NIDAQmxManager_type*) callbackData;
	int 					error 		= 0;
	int						nDev		= 0;								// number of DAQ devices found
	
	if (!devList)
		nullChk(devList	= ListCreate(sizeof(DevAttr_type*)));
	
	// clear table
	DeleteTableRows(nidaq->devListPanHndl, DevListPan_DAQTable, 1, -1);
	// display device properties table
	DisplayPanel(nidaq->devListPanHndl);
	
	// find devices
	SetCtrlAttribute (nidaq->devListPanHndl, DevListPan_DAQTable, ATTR_LABEL_TEXT,"Searching for devices...");
	ProcessDrawEvents();
	errChk(nDev = init_DevList(devList, nidaq->devListPanHndl, DevListPan_DAQTable));
	
	if (nDev > 0)
		SetCtrlAttribute(nidaq->devListPanHndl, DevListPan_AddBTTN, ATTR_DIMMED, 0); 
		
	
	return;
Error:
	
	if (devList) {ListDispose(devList); devList = 0;}
	
	return;
}

static void CVICALLBACK DeleteDev_CB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle)
{
	NIDAQmxManager_type* 	nidaq 			= (NIDAQmxManager_type*) callbackData;
	Dev_type**				DAQmxDevPtrPtr;
	int						activeTabIdx;		// 0-based index
	int						nTabPages;

	// get selected tab index
	GetActiveTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, &activeTabIdx);
	// get pointer to selected DAQmx device and remove its Task Controller from the framework
	DAQmxDevPtrPtr = ListGetPtrToItem(nidaq->DAQmxDevices, activeTabIdx + 1);
	
	// remove from framework
	DLRemoveTaskController((DAQLabModule_type*)nidaq, (*DAQmxDevPtrPtr)->taskController); 
	// discard DAQmx device data
	discard_Dev_type(DAQmxDevPtrPtr);
	ListRemoveItem(nidaq->DAQmxDevices, 0, activeTabIdx + 1);
	
	// remove Tab and add an empty "None" tab page if there are no more tabs
	DeleteTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, activeTabIdx, 1);
	GetNumTabPages(nidaq->mainPanHndl, NIDAQmxPan_Devices, &nTabPages);
	if (!nTabPages) {
		InsertTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, 0, "None");
		// dim "Delete" menu item
		SetMenuBarAttribute(nidaq->menuBarHndl, nidaq->deleteDevMenuItemID, ATTR_DIMMED, 1);
	}
	
}

static int CVICALLBACK TaskStartTrigType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
			
		TaskTrig_type* taskTrigPtr = callbackData; 
			
		case EVENT_COMMIT:
			
			// get trigger type
			GetCtrlVal(panel, control, &taskTrigPtr->trigType);
			
			// clear all possible controls except the select trigger type control
			if (taskTrigPtr->levelCtrlID) 			{DiscardCtrl(panel, taskTrigPtr->levelCtrlID); taskTrigPtr->levelCtrlID = 0;} 
			if (taskTrigPtr->trigSlopeCtrlID)		{DiscardCtrl(panel, taskTrigPtr->trigSlopeCtrlID); taskTrigPtr->trigSlopeCtrlID = 0;} 
			if (taskTrigPtr->trigSourceCtrlID)		{NIDAQmx_DiscardIOCtrl(panel, taskTrigPtr->trigSourceCtrlID); taskTrigPtr->trigSourceCtrlID = 0;} 
			if (taskTrigPtr->windowTrigCondCtrlID)	{DiscardCtrl(panel, taskTrigPtr->windowTrigCondCtrlID); taskTrigPtr->windowTrigCondCtrlID = 0;} 
			if (taskTrigPtr->windowBottomCtrlID)	{DiscardCtrl(panel, taskTrigPtr->windowBottomCtrlID); taskTrigPtr->windowBottomCtrlID = 0;} 
			if (taskTrigPtr->windowTopCtrlID)		{DiscardCtrl(panel, taskTrigPtr->windowTopCtrlID); taskTrigPtr->windowTopCtrlID = 0;} 
			
			// load resources
			int DigEdgeTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig1);
			int DigPatternTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig2);
			int AnEdgeTrig_PanHndl	 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig3);
			int AnWindowTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig4);
			
			// add controls based on the selected trigger type
			switch (taskTrigPtr->trigType) {
					
				case Trig_None:
					break;
					
				case Trig_DigitalEdge:
					
					// trigger slope
					taskTrigPtr->trigSlopeCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, StartTrig1_Slope, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
					{int ctrlIdx;
					GetIndexFromValue(panel, taskTrigPtr->trigSlopeCtrlID, &ctrlIdx, taskTrigPtr->slope);
					SetCtrlIndex(panel, taskTrigPtr->trigSlopeCtrlID, ctrlIdx);}
					// trigger source
					taskTrigPtr->trigSourceCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, StartTrig1_Source, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
					// turn string control into terminal control to select the trigger source
					NIDAQmx_NewTerminalCtrl(panel, taskTrigPtr->trigSourceCtrlID, 0); // single terminal selection 
					// adjust trigger terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					break;
					
				case Trig_DigitalPattern:
					
					// not yet implemented, but similarly, here controls should be copied to the new panel
					break;
					
				case Trig_AnalogEdge:
					
					// trigger slope
					taskTrigPtr->trigSlopeCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, StartTrig3_Slope, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
					{int ctrlIdx;
					GetIndexFromValue(panel, taskTrigPtr->trigSlopeCtrlID, &ctrlIdx, taskTrigPtr->slope);
					SetCtrlIndex(panel, taskTrigPtr->trigSlopeCtrlID, ctrlIdx);}
					// level
					taskTrigPtr->levelCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, StartTrig3_Level, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->levelCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->levelCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerLevel_CB );
					// trigger source
					taskTrigPtr->trigSourceCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, StartTrig3_Source, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
					// turn string control into terminal control to select the trigger source
					NIDAQmx_NewTerminalCtrl(panel, taskTrigPtr->trigSourceCtrlID, 0); // single terminal selection 
					// adjust trigger terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					break;
					
				case Trig_AnalogWindow:
					
					// window type
					taskTrigPtr->windowTrigCondCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_WndType, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
					SetCtrlAttribute(panel, taskTrigPtr->windowTrigCondCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->windowTrigCondCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowType_CB);
					InsertListItem(panel, taskTrigPtr->windowTrigCondCtrlID, 0, "Entering", TrigWnd_Entering); 
					InsertListItem(panel, taskTrigPtr->windowTrigCondCtrlID, 1, "Leaving", TrigWnd_Leaving);
					{int ctrlIdx;
					GetIndexFromValue(panel, taskTrigPtr->windowTrigCondCtrlID, &ctrlIdx, taskTrigPtr->wndTrigCond);
					SetCtrlIndex(panel, taskTrigPtr->windowTrigCondCtrlID, ctrlIdx);}
					// window top
					taskTrigPtr->windowTopCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_TrigWndTop, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
					SetCtrlAttribute(panel, taskTrigPtr->windowTopCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->windowTopCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowTop_CB);
					// window bottom
					taskTrigPtr->windowBottomCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_TrigWndBttm, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
					SetCtrlAttribute(panel, taskTrigPtr->windowBottomCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->windowBottomCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowBttm_CB);
					// trigger source
					taskTrigPtr->trigSourceCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_Source, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
					// turn string control into terminal control to select the trigger source
					NIDAQmx_NewTerminalCtrl(panel, taskTrigPtr->trigSourceCtrlID, 0); // single terminal selection 
					// adjust trigger terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					break;
			}
			
			// discard resources
			DiscardPanel(DigEdgeTrig_PanHndl);
			DiscardPanel(DigPatternTrig_PanHndl);
			DiscardPanel(AnEdgeTrig_PanHndl);
			DiscardPanel(AnWindowTrig_PanHndl);
			
			break;
	}
	
	return 0;
}

static int CVICALLBACK TaskReferenceTrigType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
			
		TaskTrig_type* taskTrigPtr = callbackData; 
			
		case EVENT_COMMIT:
			
			// get trigger type
			int trigType;
			GetCtrlVal(panel, control, &trigType);
			taskTrigPtr->trigType = trigType;
			
			// clear all possible controls except the select trigger type control
			if (taskTrigPtr->levelCtrlID) 			{DiscardCtrl(panel, taskTrigPtr->levelCtrlID); taskTrigPtr->levelCtrlID = 0;} 
			if (taskTrigPtr->preTrigDurationCtrlID) {DiscardCtrl(panel, taskTrigPtr->preTrigDurationCtrlID); taskTrigPtr->preTrigDurationCtrlID = 0;} 
			if (taskTrigPtr->preTrigNSamplesCtrlID) {DiscardCtrl(panel, taskTrigPtr->preTrigNSamplesCtrlID); taskTrigPtr->preTrigNSamplesCtrlID = 0;} 
			if (taskTrigPtr->trigSlopeCtrlID)		{DiscardCtrl(panel, taskTrigPtr->trigSlopeCtrlID); taskTrigPtr->trigSlopeCtrlID = 0;} 
			if (taskTrigPtr->trigSourceCtrlID)		{NIDAQmx_DiscardIOCtrl(panel, taskTrigPtr->trigSourceCtrlID); taskTrigPtr->trigSourceCtrlID = 0;} 
			if (taskTrigPtr->windowTrigCondCtrlID)	{DiscardCtrl(panel, taskTrigPtr->windowTrigCondCtrlID); taskTrigPtr->windowTrigCondCtrlID = 0;} 
			if (taskTrigPtr->windowBottomCtrlID)	{DiscardCtrl(panel, taskTrigPtr->windowBottomCtrlID); taskTrigPtr->windowBottomCtrlID = 0;} 
			if (taskTrigPtr->windowTopCtrlID)		{DiscardCtrl(panel, taskTrigPtr->windowTopCtrlID); taskTrigPtr->windowTopCtrlID = 0;} 
			
			// load resources
			int DigEdgeTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig1);
			int DigPatternTrig_PanHndl	 	= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig2);
			int AnEdgeTrig_PanHndl	 		= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig3);
			int AnWindowTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig4);
			
			// add controls based on the selected trigger type
			switch (taskTrigPtr->trigType) {
					
				case Trig_None:
					break;
					
				case Trig_DigitalEdge:
					
					// trigger slope
					taskTrigPtr->trigSlopeCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_Slope, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
					{int ctrlIdx;
					GetIndexFromValue(panel, taskTrigPtr->trigSlopeCtrlID, &ctrlIdx, taskTrigPtr->slope);
					SetCtrlIndex(panel, taskTrigPtr->trigSlopeCtrlID, ctrlIdx);}
					// pre-trigger samples
					taskTrigPtr->preTrigNSamplesCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_NSamples, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_SIZE_T);
					SetCtrlVal(panel, taskTrigPtr->preTrigNSamplesCtrlID, taskTrigPtr->nPreTrigSamples);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigSamples_CB);
					// pre-trigger duration
					taskTrigPtr->preTrigDurationCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_Duration, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlVal(panel, taskTrigPtr->preTrigDurationCtrlID, taskTrigPtr->nPreTrigSamples/(*taskTrigPtr->samplingRate));  // display in [s]
					SetCtrlAttribute(panel, taskTrigPtr->preTrigDurationCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigDurationCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigDuration_CB);
					// trigger source
					taskTrigPtr->trigSourceCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_Source, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
					// turn string control into terminal control to select the trigger source
					NIDAQmx_NewTerminalCtrl(panel, taskTrigPtr->trigSourceCtrlID, 0); // single terminal selection 
					// adjust trigger terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					break;
					
				case Trig_DigitalPattern:
					
					// not yet implemented, but similarly, here controls should be copied to the new panel
					break;
					
				case Trig_AnalogEdge:
					
					// trigger slope
					taskTrigPtr->trigSlopeCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig3_Slope, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
					InsertListItem(panel, taskTrigPtr->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
					{int ctrlIdx;
					GetIndexFromValue(panel, taskTrigPtr->trigSlopeCtrlID, &ctrlIdx, taskTrigPtr->slope);
					SetCtrlIndex(panel, taskTrigPtr->trigSlopeCtrlID, ctrlIdx);}
					// level
					taskTrigPtr->levelCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_Level, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->levelCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->levelCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerLevel_CB );
					// pre-trigger samples
					taskTrigPtr->preTrigNSamplesCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_NSamples, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_SIZE_T);
					SetCtrlVal(panel, taskTrigPtr->preTrigNSamplesCtrlID, taskTrigPtr->nPreTrigSamples);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigSamples_CB);
					// pre-trigger duration
					taskTrigPtr->preTrigDurationCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_Duration, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlVal(panel, taskTrigPtr->preTrigDurationCtrlID, taskTrigPtr->nPreTrigSamples/(*taskTrigPtr->samplingRate));  // display in [s]
					SetCtrlAttribute(panel, taskTrigPtr->preTrigDurationCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigDurationCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigDuration_CB);
					// trigger source
					taskTrigPtr->trigSourceCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_Source, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
					// turn string control into terminal control to select the trigger source
					NIDAQmx_NewTerminalCtrl(panel, taskTrigPtr->trigSourceCtrlID, 0); // single terminal selection 
					// adjust trigger terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					break;
					
				case Trig_AnalogWindow:
					
					// window type
					taskTrigPtr->windowTrigCondCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_WndType, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
					SetCtrlAttribute(panel, taskTrigPtr->windowTrigCondCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->windowTrigCondCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowType_CB);
					InsertListItem(panel, taskTrigPtr->windowTrigCondCtrlID, 0, "Entering", TrigWnd_Entering); 
					InsertListItem(panel, taskTrigPtr->windowTrigCondCtrlID, 1, "Leaving", TrigWnd_Leaving);
					{int ctrlIdx;
					GetIndexFromValue(panel, taskTrigPtr->windowTrigCondCtrlID, &ctrlIdx, taskTrigPtr->wndTrigCond);
					SetCtrlIndex(panel, taskTrigPtr->windowTrigCondCtrlID, ctrlIdx);}
					// window top
					taskTrigPtr->windowTopCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_TrigWndTop, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
					SetCtrlAttribute(panel, taskTrigPtr->windowTopCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->windowTopCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowTop_CB);
					// window bottom
					taskTrigPtr->windowBottomCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_TrigWndBttm, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
					SetCtrlAttribute(panel, taskTrigPtr->windowBottomCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->windowBottomCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowBttm_CB);
					// pre-trigger samples
					taskTrigPtr->preTrigNSamplesCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_NSamples, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_SIZE_T);
					SetCtrlVal(panel, taskTrigPtr->preTrigNSamplesCtrlID, taskTrigPtr->nPreTrigSamples);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigSamples_CB);
					// pre-trigger duration
					taskTrigPtr->preTrigDurationCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_Duration, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlVal(panel, taskTrigPtr->preTrigDurationCtrlID, taskTrigPtr->nPreTrigSamples/(*taskTrigPtr->samplingRate));  // display in [s]
					SetCtrlAttribute(panel, taskTrigPtr->preTrigDurationCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->preTrigDurationCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigDuration_CB);
					// trigger source
					taskTrigPtr->trigSourceCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_Source, panel, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrigPtr);
					SetCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
					// turn string control into terminal control to select the trigger source
					NIDAQmx_NewTerminalCtrl(panel, taskTrigPtr->trigSourceCtrlID, 0); // single terminal selection 
					// adjust trigger terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
					NIDAQmx_SetTerminalCtrlAttribute(panel, taskTrigPtr->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					break;
			}
			
			// discard resources
			DiscardPanel(DigEdgeTrig_PanHndl);
			DiscardPanel(DigPatternTrig_PanHndl);
			DiscardPanel(AnEdgeTrig_PanHndl);
			DiscardPanel(AnWindowTrig_PanHndl);
			
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerSlope_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* taskTrigPtr = callbackData; 
			GetCtrlVal(panel, control, &taskTrigPtr->slope);
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerLevel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* taskTrigPtr = callbackData;
			GetCtrlVal(panel, control, &taskTrigPtr->level);
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerSource_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* 	taskTrigPtr = callbackData; 
			int 		  	buffsize	= 0;
			
			GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
			OKfree(taskTrigPtr->trigSource);
			taskTrigPtr->trigSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null  
			GetCtrlVal(panel, control, taskTrigPtr->trigSource);
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerWindowType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* taskTrigPtr = callbackData;   
			GetCtrlVal(panel, control, &taskTrigPtr->wndTrigCond);
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerWindowBttm_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* taskTrigPtr = callbackData;
			GetCtrlVal(panel, control, &taskTrigPtr->wndBttm);
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerWindowTop_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* taskTrigPtr = callbackData;   
			GetCtrlVal(panel, control, &taskTrigPtr->wndTop);
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerPreTrigDuration_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* taskTrigPtr = callbackData;
			double	duration;
			GetCtrlVal(panel, control, &duration);
			// calculate number of samples given sampling rate
			taskTrigPtr->nPreTrigSamples = (size_t) (*taskTrigPtr->samplingRate * duration);
			SetCtrlVal(panel, taskTrigPtr->preTrigNSamplesCtrlID, taskTrigPtr->nPreTrigSamples);
			// recalculate duration to match the number of samples and sampling rate
			SetCtrlVal(panel, control, taskTrigPtr->nPreTrigSamples/(*taskTrigPtr->samplingRate)); 
			break;
	}
	
	return 0;
}

static int CVICALLBACK TriggerPreTrigSamples_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_COMMIT:
			
			TaskTrig_type* taskTrigPtr = callbackData;   
			GetCtrlVal(panel, control, &taskTrigPtr->nPreTrigSamples);
			// recalculate duration
			SetCtrlVal(panel, taskTrigPtr->preTrigDurationCtrlID, taskTrigPtr->nPreTrigSamples/(*taskTrigPtr->samplingRate)); 
			break;
	}
	
	return 0;
}

static void PopulateIOMode (Dev_type* dev, int panHndl, int controlID, int ioVal)
{
	ClearListCtrl(panHndl, controlID); 
	
	switch (ioVal) {
			
		case DAQmxAcquire:
							
			// analog
			if (ListNumItems(dev->attr->AIchan)) 
				InsertListItem(panHndl, controlID, -1, "Analog", DAQmxAnalog); 
			// digital
			if (ListNumItems(dev->attr->DIports) || ListNumItems(dev->attr->DIlines)) 
				InsertListItem(panHndl, controlID, -1, "Digital", DAQmxDigital); 
			// counter
			if (ListNumItems(dev->attr->CIchan)) 
				InsertListItem(panHndl, controlID, -1, "Counter", DAQmxCounter); 
						
			break;
							
		case DAQmxGenerate:
							
			// analog
			if (ListNumItems(dev->attr->AOchan)) 
				InsertListItem(panHndl, controlID, -1, "Analog", DAQmxAnalog); 
			// digital
			if (ListNumItems(dev->attr->DOports) || ListNumItems(dev->attr->DOlines)) 
				InsertListItem(panHndl, controlID, -1, "Digital", DAQmxDigital); 
			// counter
			if (ListNumItems(dev->attr->COchan)) 
				InsertListItem(panHndl, controlID, -1, "Counter", DAQmxCounter); 
							
			break;
	}
}

static void PopulateIOType (Dev_type* dev, int panHndl, int controlID, int ioVal, int ioMode)
{
	size_t	n;
	int*	measTypePtr;
	
	ClearListCtrl(panHndl, controlID); 
	
	switch (ioVal) {
			
		case DAQmxAcquire:
			
			switch (ioMode) {
					
				case DAQmxAnalog:
					
					n = ListNumItems(dev->attr->AISupportedMeasTypes);
					for (size_t i = 1; i <= n; i++) {
						measTypePtr = ListGetPtrToItem(dev->attr->AISupportedMeasTypes, i);
						switch (*measTypePtr) {
							// Voltage measurement.
							case DAQmx_Val_Voltage:  		
								InsertListItem(panHndl, controlID, -1, "Voltage", DAQmx_Val_Voltage); 
								break;
							// Voltage RMS measurement.
							case DAQmx_Val_VoltageRMS: 		
								InsertListItem(panHndl, controlID, -1, "Voltage RMS", DAQmx_Val_VoltageRMS); 
								break;
							// Current measurement.
							case DAQmx_Val_Current:			
								InsertListItem(panHndl, controlID, -1, "Current", DAQmx_Val_Current); 
								break;
							// Current RMS measurement.
							case DAQmx_Val_CurrentRMS:		
								InsertListItem(panHndl, controlID, -1, "Current RMS", DAQmx_Val_CurrentRMS); 
								break;
							// Voltage measurement with an excitation source. 
							// You can use this measurement type for custom sensors that require excitation, but you must use a custom scale to scale the measured voltage.
							case DAQmx_Val_Voltage_CustomWithExcitation:
								InsertListItem(panHndl, controlID, -1, "Voltage with excitation", DAQmx_Val_Voltage_CustomWithExcitation); 
								break;
							// Measure voltage ratios from a Wheatstone bridge.
							case DAQmx_Val_Bridge:
								InsertListItem(panHndl, controlID, -1, "Bridge", DAQmx_Val_Bridge); 
								break;
							// Frequency measurement using a frequency to voltage converter.
							case DAQmx_Val_Freq_Voltage:
								InsertListItem(panHndl, controlID, -1, "Frequency from voltage",DAQmx_Val_Freq_Voltage); 
								break;
							// Resistance measurement.
							case DAQmx_Val_Resistance:
								InsertListItem(panHndl, controlID, -1, "Resistance", DAQmx_Val_Resistance); 
								break;
							// Temperature measurement using a thermocouple. 
							case DAQmx_Val_Temp_TC:
								InsertListItem(panHndl, controlID, -1, "Temperature with thermocouple", DAQmx_Val_Temp_TC); 
								break;
							// Temperature measurement using a thermistor.
							case DAQmx_Val_Temp_Thrmstr:
								InsertListItem(panHndl, controlID, -1, "Temperature with thermistor", DAQmx_Val_Temp_Thrmstr); 
								break;
							// Temperature measurement using an RTD.
							case DAQmx_Val_Temp_RTD:
								InsertListItem(panHndl, controlID, -1, "Temperature with RTD", DAQmx_Val_Temp_RTD); 
								break;
							// Temperature measurement using a built-in sensor on a terminal block or device. On SCXI modules, for example, this could be the CJC sensor. 
							case DAQmx_Val_Temp_BuiltInSensor:
								InsertListItem(panHndl, controlID, -1, "Temperature with built in sensor", DAQmx_Val_Temp_BuiltInSensor); 
								break;
							// Strain measurement.
							case DAQmx_Val_Strain_Gage:
								InsertListItem(panHndl, controlID, -1, "Strain gage", DAQmx_Val_Strain_Gage); 
								break;
							// Strain measurement using a rosette strain gage.
							case DAQmx_Val_Rosette_Strain_Gage :
								InsertListItem(panHndl, controlID, -1, "Strain with rosette strain gage", DAQmx_Val_Rosette_Strain_Gage ); 
								break;
							// Position measurement using an LVDT.
							case DAQmx_Val_Position_LVDT:
								InsertListItem(panHndl, controlID, -1, "Position using an LVDT", DAQmx_Val_Position_LVDT); 
								break;
							// Position measurement using an RVDT. 
							case DAQmx_Val_Position_RVDT:
								InsertListItem(panHndl, controlID, -1, "Position using an RVDT", DAQmx_Val_Position_RVDT); 
								break;
							// Position measurement using an eddy current proximity probe.
							case DAQmx_Val_Position_EddyCurrentProximityProbe:
								InsertListItem(panHndl, controlID, -1, "Position using eddy current proximity probe", DAQmx_Val_Position_EddyCurrentProximityProbe); 
								break;
							// Acceleration measurement using an accelerometer.
							case DAQmx_Val_Accelerometer:
								InsertListItem(panHndl, controlID, -1, "Acceleration with an accelerometer", DAQmx_Val_Accelerometer); 
								break;
							// Velocity measurement using an IEPE Sensor.
							case DAQmx_Val_Velocity_IEPESensor:
								InsertListItem(panHndl, controlID, -1, "Velocity with an IEPE sensor", DAQmx_Val_Velocity_IEPESensor); 
								break;
							// Force measurement using a bridge-based sensor.
							case DAQmx_Val_Force_Bridge:
								InsertListItem(panHndl, controlID, -1, "Force using a bridge-based sensor", DAQmx_Val_Force_Bridge); 
								break;
							// Force measurement using an IEPE Sensor.
							case DAQmx_Val_Force_IEPESensor:
								InsertListItem(panHndl, controlID, -1, "Force using an IEPE sensor", DAQmx_Val_Force_IEPESensor); 
								break;
							// Pressure measurement using a bridge-based sensor.
							case DAQmx_Val_Pressure_Bridge:
								InsertListItem(panHndl, controlID, -1, "Pressure using a bridge-based sensor", DAQmx_Val_Pressure_Bridge); 
								break;
							// Sound pressure measurement using a microphone.
							case DAQmx_Val_SoundPressure_Microphone:
								InsertListItem(panHndl, controlID, -1, "Sound pressure using a microphone", DAQmx_Val_SoundPressure_Microphone); 
								break;
							// Torque measurement using a bridge-based sensor.
							case DAQmx_Val_Torque_Bridge:
								InsertListItem(panHndl, controlID, -1, "Torque using a bridge-based sensor", DAQmx_Val_Torque_Bridge); 
								break;
							// Measurement type defined by TEDS.
							case DAQmx_Val_TEDS_Sensor:
								InsertListItem(panHndl, controlID, -1, "TEDS based measurement", DAQmx_Val_TEDS_Sensor); 
								break;	
						}
					}
					
					// select Voltage by default if there is such an option
					int voltageIdx;
					if (GetIndexFromValue(panHndl, controlID, &voltageIdx, DAQmx_Val_Voltage) >= 0)
						SetCtrlIndex(panHndl, controlID, voltageIdx);
					
					break;
					
				case DAQmxDigital:
					
					if (ListNumItems(dev->attr->DIlines))
						InsertListItem(panHndl, controlID, -1, "Lines", DAQmxDigLines);
					
					if (ListNumItems(dev->attr->DIports))
						InsertListItem(panHndl, controlID, -1, "Ports", DAQmxDigPorts);	
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->CISupportedMeasTypes);
					for (size_t i = 1; i <= n; i++) {
						measTypePtr = ListGetPtrToItem(dev->attr->CISupportedMeasTypes, i);
						switch (*measTypePtr) {
							// Count edges of a digital signal.
							case DAQmx_Val_CountEdges:  		
								InsertListItem(panHndl, controlID, -1, "Edge count", DAQmx_Val_CountEdges); 
								break;
							// Measure the frequency of a digital signal.
							case DAQmx_Val_Freq: 		
								InsertListItem(panHndl, controlID, -1, "Frequency", DAQmx_Val_Freq); 
								break;
							// Measure the period of a digital signal.
							case DAQmx_Val_Period:			
								InsertListItem(panHndl, controlID, -1, "Period", DAQmx_Val_Period); 
								break;
							// Measure the width of a pulse of a digital signal.
							case DAQmx_Val_PulseWidth:		
								InsertListItem(panHndl, controlID, -1, "Pulse width", DAQmx_Val_PulseWidth); 
								break;
							// Measure the time between state transitions of a digital signal.
							case DAQmx_Val_SemiPeriod:
								InsertListItem(panHndl, controlID, -1, "Semi period", DAQmx_Val_SemiPeriod); 
								break;
							// Pulse measurement, returning the result as frequency and duty cycle. 
							case DAQmx_Val_PulseFrequency:
								InsertListItem(panHndl, controlID, -1, "Pulse frequency", DAQmx_Val_PulseFrequency); 
								break;
							// Pulse measurement, returning the result as high time and low time.
							case DAQmx_Val_PulseTime:
								InsertListItem(panHndl, controlID, -1, "Pulse time", DAQmx_Val_PulseTime); 
								break;
							// Pulse measurement, returning the result as high ticks and low ticks.
							case DAQmx_Val_PulseTicks:
								InsertListItem(panHndl, controlID, -1, "Pulse ticks", DAQmx_Val_PulseTicks); 
								break;
							// Angular position measurement using an angular encoder.
							case DAQmx_Val_Position_AngEncoder:
								InsertListItem(panHndl, controlID, -1, "Position with angular encoder", DAQmx_Val_Position_AngEncoder); 
								break;
							// Linear position measurement using a linear encoder.
							case DAQmx_Val_Position_LinEncoder:
								InsertListItem(panHndl, controlID, -1, "Position with linear encoder", DAQmx_Val_Position_LinEncoder); 
								break;
							// Measure time between edges of two digital signals.
							case DAQmx_Val_TwoEdgeSep:
								InsertListItem(panHndl, controlID, -1, "Two edge step", DAQmx_Val_TwoEdgeSep); 
								break;
							// Timestamp measurement, synchronizing the counter to a GPS receiver. 
							case DAQmx_Val_GPS_Timestamp:
								InsertListItem(panHndl, controlID, -1, "GPS timestamp", DAQmx_Val_GPS_Timestamp); 
								break;
						}
					}
					
					break;
			}
			break;
			
		case DAQmxGenerate:
			
			switch (ioMode) {
					
				case DAQmxAnalog:
					
					n = ListNumItems(dev->attr->AOSupportedOutputTypes);
					for (size_t i = 1; i <= n; i++) {
						measTypePtr = ListGetPtrToItem(dev->attr->AOSupportedOutputTypes, i);
						switch (*measTypePtr) {
							// Voltage generation.
							case DAQmx_Val_Voltage:
								InsertListItem(panHndl, controlID, -1, "Voltage", DAQmx_Val_Voltage); 
								break;
							// Current generation.
							case DAQmx_Val_Current:
								InsertListItem(panHndl, controlID, -1, "Current", DAQmx_Val_Current); 
								break;
							// Function generation. 
							case DAQmx_Val_FuncGen:
								InsertListItem(panHndl, controlID, -1, "Function", DAQmx_Val_FuncGen); 
								break;
						}
					}
					// select Voltage by default if there is such an option
					int voltageIdx;
					if (GetIndexFromValue(panHndl, controlID, &voltageIdx, DAQmx_Val_Voltage) >= 0)
						SetCtrlIndex(panHndl, controlID, voltageIdx);
					
					break;
					
				case DAQmxDigital:
					
					if (ListNumItems(dev->attr->DOlines))
						InsertListItem(panHndl, controlID, -1, "Lines", DAQmxDigLines);
					
					if (ListNumItems(dev->attr->DOports))
						InsertListItem(panHndl, controlID, -1, "Ports", DAQmxDigPorts);	
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->COSupportedOutputTypes);
					for (size_t i = 1; i <= n; i++) {
						measTypePtr = ListGetPtrToItem(dev->attr->COSupportedOutputTypes, i);
						switch (*measTypePtr) {
							// Generate pulses defined by the time the pulse is at a low state and the time the pulse is at a high state.
							case DAQmx_Val_Pulse_Time:
								InsertListItem(panHndl, controlID, -1, "Pulse by time", DAQmx_Val_Pulse_Time); 
								break;
							// Generate digital pulses defined by frequency and duty cycle.
							case DAQmx_Val_Pulse_Freq:
								InsertListItem(panHndl, controlID, -1, "Pulse by frequency", DAQmx_Val_Pulse_Freq); 
								break;
							// Generate digital pulses defined by the number of timebase ticks that the pulse is at a low state and the number of timebase ticks that the pulse is at a high state. 
							case DAQmx_Val_Pulse_Ticks:
								InsertListItem(panHndl, controlID, -1, "Pulse by ticks", DAQmx_Val_Pulse_Ticks); 
								break;
						}
					}
					
					break;
			}
			
			break;
	}
}

static int DAQmxDevTaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type* 		dev	 		= (Dev_type*) callbackData;
	int				ioVal;
	int				ioMode;
	int				ioType;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case TaskSetPan_IO:
					
					GetCtrlVal(panel, TaskSetPan_IO, &ioVal);
					PopulateIOMode(dev, panel, TaskSetPan_IOMode, ioVal); 
					GetCtrlVal(panel, TaskSetPan_IOMode, &ioMode);
					PopulateIOType(dev, panel, TaskSetPan_IOType, ioVal, ioMode);
					PopulateChannels (dev);
					break;
					
				case TaskSetPan_IOMode:
					
					GetCtrlVal(panel, TaskSetPan_IO, &ioVal);
					GetCtrlVal(panel, TaskSetPan_IOMode, &ioMode);
					PopulateIOType(dev, panel, TaskSetPan_IOType, ioVal, ioMode);
					PopulateChannels (dev);
					break;
					
				case TaskSetPan_IOType:
					
					PopulateChannels (dev);
					break;
					
				case TaskSetPan_Mode:
				case TaskSetPan_Repeat:
				case TaskSetPan_Wait:
					
					TaskControlEvent(dev->taskController, TASK_EVENT_CONFIGURE, NULL, NULL);
					break;
					
			}
			break;
			
		case EVENT_LEFT_DOUBLE_CLICK: // add channel and create DAQmx task
			
			switch (control) {
					
				case TaskSetPan_PhysChan:
					
					int		itemIdx;
					char*	chanName	= NULL;
					int		buffsize;
					
					GetCtrlIndex(panel, control, &itemIdx);
					// continue if there are any channels to add
					if (itemIdx < 0) break;
					
					// get physical channel name
					GetValueLengthFromIndex(panel, control, itemIdx, &buffsize);
					chanName = malloc((buffsize+1) * sizeof(char)); // including ASCII null  
					GetValueFromIndex(panel, control, itemIdx, chanName);
					
					GetCtrlVal(panel, TaskSetPan_IO, &ioVal);
					GetCtrlVal(panel, TaskSetPan_IOMode, &ioMode);
					GetCtrlVal(panel, TaskSetPan_IOType, &ioType);
					
					// add channel
					AddDAQmxChannel(dev, ioVal, ioMode, ioType, chanName);
					
					// refresh available channels
					PopulateChannels(dev);
					
					OKfree(chanName);
					break;
			}
			break;
			
		case EVENT_KEYPRESS:
			
			switch (control) {
					
				case TaskSetPan_DAQTasks: // remove selected DAQmx task and all its channels
					
					// continue only if Del key is pressed
					if (eventData1 != VAL_FWD_DELETE_VKEY) break;
					
					
					
					
					break;
			}
			break;
	}
	
	return 0;
}

static int RemoveDAQmxAIChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_KEYPRESS: 
			
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			Dev_type*	dev = callbackData; 
			
			int tabIdx;
			GetActiveTabPage(panel, control, &tabIdx);
			int chanTabPanHndl;
			GetPanelHandleFromTabPage(panel, control, tabIdx, &chanTabPanHndl);
			ChanSet_type* aiChanPtr;
			GetPanelAttribute(chanTabPanHndl, ATTR_CALLBACK_DATA, &aiChanPtr);
			// if this is the "None" labelled tab stop here
			if (!aiChanPtr) break;
				
			// mark channel as available again
			AIChannel_type* AIChanAttr 	= GetAIChannel(dev, aiChanPtr->name);
			AIChanAttr->inUse = FALSE;
			// remove channel from AI task
			ChanSet_type** 	chanSetPtrPtr;
			size_t			nItems			= ListNumItems(dev->AITaskSet->chanSet);
			size_t			chIdx			= 1;
			for (size_t i = 1; i <= nItems; i++) {	
				chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
				if (*chanSetPtrPtr == aiChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtrPtr)->srcVChan);
					// discard channel data structure
					(*(*chanSetPtrPtr)->discardFptr)	(chanSetPtrPtr);
					ListRemoveItem(dev->AITaskSet->chanSet, 0, chIdx);
					break;
				}
				chIdx++;
			}
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove AI task
			if (!nTabs) {
				discard_AIAOTaskSet_type(&dev->AITaskSet);
				int tabIdx;
				GetActiveTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, &tabIdx);
				DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, tabIdx, 1);
				GetNumTabPages(dev->devPanHndl, TaskSetPan_DAQTasks, &nTabs);
				// if there are no more tabs, add back the "None" labelled tab
				if (!nTabs)
					InsertTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, "None");
			}
			
			// refresh channel list
			PopulateChannels(dev);
				
			break;
	}
	
	return 0;
}

static int RemoveDAQmxAOChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_KEYPRESS: 
			
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			Dev_type*	dev = callbackData; 
			
			int tabIdx;
			GetActiveTabPage(panel, control, &tabIdx);
			int chanTabPanHndl;
			GetPanelHandleFromTabPage(panel, control, tabIdx, &chanTabPanHndl);
			ChanSet_type* aoChanPtr;
			GetPanelAttribute(chanTabPanHndl, ATTR_CALLBACK_DATA, &aoChanPtr);
			// if this is the "None" labelled tab stop here
			if (!aoChanPtr) break;
				
			// mark channel as available again
			AOChannel_type* AOChanAttr 	= GetAOChannel(dev, aoChanPtr->name);
			AOChanAttr->inUse = FALSE;
			// remove channel from AO task
			ChanSet_type** 	chanSetPtrPtr;
			size_t			nItems			= ListNumItems(dev->AOTaskSet->chanSet);
			size_t			chIdx			= 1;
			for (size_t i = 1; i <= nItems; i++) {	
				chanSetPtrPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
				if (*chanSetPtrPtr == aoChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtrPtr)->sinkVChan);
					// detach from Task Controller										 
					RemoveSinkVChan(dev->taskController, (*chanSetPtrPtr)->sinkVChan);
					// discard channel data structure
					(*(*chanSetPtrPtr)->discardFptr)	(chanSetPtrPtr);
					ListRemoveItem(dev->AOTaskSet->chanSet, 0, chIdx);
					break;
				}
				chIdx++;
			}
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove AO task
			if (!nTabs) {
				discard_AIAOTaskSet_type(&dev->AOTaskSet);
				int tabIdx;
				GetActiveTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, &tabIdx);
				DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, tabIdx, 1);
				GetNumTabPages(dev->devPanHndl, TaskSetPan_DAQTasks, &nTabs);
				// if there are no more tabs, add back the "None" labelled tab
				if (!nTabs)
					InsertTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, "None");
			}
			
			// refresh channel list
			PopulateChannels(dev);
				
			break;
	}
	
	return 0;
}


static int RemoveDAQmxCIChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_KEYPRESS: 
			
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			Dev_type*	dev = callbackData; 
			
			int tabIdx;
			GetActiveTabPage(panel, control, &tabIdx);
			int chanTabPanHndl;
			GetPanelHandleFromTabPage(panel, control, tabIdx, &chanTabPanHndl);
			ChanSet_type* ciChanPtr;
			GetPanelAttribute(chanTabPanHndl, ATTR_CALLBACK_DATA, &ciChanPtr);
			// if this is the "None" labelled tab stop here
			if (!ciChanPtr) break;
				
			// mark channel as available again
			CIChannel_type* CIChanAttr 	= GetCIChannel(dev, ciChanPtr->name);
			CIChanAttr->inUse = FALSE;
			// remove channel from CI task
	/*		ChanSet_type** 	chanSetPtrPtr;
			size_t			nItems			= ListNumItems(dev->CITaskSet->chanSet);
			size_t			chIdx			= 1;
			for (size_t i = 1; i <= nItems; i++) {	
				chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanSet, i);
				if (*chanSetPtrPtr == ciChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtrPtr)->sinkVChan);
					// detach from Task Controller										 
					RemoveSinkVChan(dev->taskController, (*chanSetPtrPtr)->sinkVChan);
					// discard channel data structure
					(*(*chanSetPtrPtr)->discardFptr)	(chanSetPtrPtr);
					ListRemoveItem(dev->CITaskSet->chanSet, 0, chIdx);
					break;
				}
				chIdx++;
			}		   */
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove CI task
			if (!nTabs) {
				discard_CITaskSet_type(&dev->CITaskSet);
				int tabIdx;
				GetActiveTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, &tabIdx);
				DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, tabIdx, 1);
				GetNumTabPages(dev->devPanHndl, TaskSetPan_DAQTasks, &nTabs);
				// if there are no more tabs, add back the "None" labelled tab
				if (!nTabs)
					InsertTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, "None");
			}
			
			// refresh channel list
			PopulateChannels(dev);
				
			break;
	}
	
	return 0;
}



static int RemoveDAQmxCOChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_KEYPRESS: 
			
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			Dev_type*	dev = callbackData; 
			
			int tabIdx;
			GetActiveTabPage(panel, control, &tabIdx);
			int chanTabPanHndl;
			GetPanelHandleFromTabPage(panel, control, tabIdx, &chanTabPanHndl);
			ChanSet_type* coChanPtr;
			GetPanelAttribute(chanTabPanHndl, ATTR_CALLBACK_DATA, &coChanPtr);
			// if this is the "None" labelled tab stop here
			if (!coChanPtr) break;
				
			// mark channel as available again
			COChannel_type* COChanAttr 	= GetCOChannel(dev, coChanPtr->name);
			COChanAttr->inUse = FALSE;
			// remove channel from CO task
			ChanSet_type** 	chanSetPtrPtr;
			size_t			nItems			= ListNumItems(dev->COTaskSet->chanTaskSet);
			size_t			chIdx			= 1;
			for (size_t i = 1; i <= nItems; i++) {	
				chanSetPtrPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
				if (*chanSetPtrPtr == coChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtrPtr)->srcVChan);
					// detach from Task Controller										 
				//	RemoveSinkVChan(dev->taskController, (*chanSetPtrPtr)->srcVChan);
					// discard channel data structure
					(*(*chanSetPtrPtr)->discardFptr)	(chanSetPtrPtr);
					ListRemoveItem(dev->COTaskSet->chanTaskSet, 0, chIdx);
					break;
				}
				chIdx++;
			}		   
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove CO task
			if (!nTabs) {
				discard_COTaskSet_type(&dev->COTaskSet);
				int tabIdx;
				GetActiveTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, &tabIdx);
				DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, tabIdx, 1);
				GetNumTabPages(dev->devPanHndl, TaskSetPan_DAQTasks, &nTabs);
				// if there are no more tabs, add back the "None" labelled tab
				if (!nTabs)
					InsertTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, "None");
			}
			
			// refresh channel list
			PopulateChannels(dev);
				
			break;
	}
	
	return 0;
}		 


int CVICALLBACK ManageDevices_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case DevListPan_AddBTTN:
					
					NIDAQmxManager_type* 	nidaq				 = (NIDAQmxManager_type*) callbackData;
					char*					newTCName;
					char*					newTabName;
					Dev_type*				newDAQmxDev;
					DevAttr_type*			newDAQmxDevAttrPtr;
					int						newDAQmxDevPanHndl;
					int						newTabPageIdx;
					int						panHndl;
					int						ioVal;
					int						ioMode;
					void*					callbackData;
					
					// create new Task Controller for the DAQmx device
					newTCName = DLGetUINameInput ("New DAQmx Task Controller", DAQLAB_MAX_TASKCONTROLLER_NAME_NCHARS, ValidTaskControllerName, NULL);
					if (!newTCName) return 0; // operation cancelled, do nothing	
					
					// copy device attributes
					newDAQmxDevAttrPtr = copy_DevAttr_type(*(DevAttr_type**)ListGetPtrToItem(devList, currDev));
					// add new DAQmx device to module list and framework
					newDAQmxDev = init_Dev_type(nidaq, &newDAQmxDevAttrPtr, newTCName);
					ListInsertItem(nidaq->DAQmxDevices, &newDAQmxDev, END_OF_LIST);
					DLAddTaskController((DAQLabModule_type*)nidaq, newDAQmxDev->taskController);
					
					// copy DAQmx Task settings panel to module tab and get handle to the panel inserted in the tab
					newTabPageIdx = InsertPanelAsTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, -1, nidaq->taskSetPanHndl);
					GetPanelHandleFromTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, newTabPageIdx, &newDAQmxDevPanHndl);
					// keep track of the DAQmx task settings panel handle
					newDAQmxDev->devPanHndl = newDAQmxDevPanHndl;
					// change tab title to new Task Controller name
					newTabName = StrDup(newDAQmxDevAttrPtr->name);
					AppendString(&newTabName, ": ", -1);
					AppendString(&newTabName, newTCName, -1);
					SetTabPageAttribute(nidaq->mainPanHndl, NIDAQmxPan_Devices, newTabPageIdx, ATTR_LABEL_TEXT, newTabName);
					free(newTabName);
					// remove "None" labelled Tab (always first tab) if its panel doesn't have callback data attached to it  
					GetPanelHandleFromTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, 0, &panHndl);
					GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
					if (!callbackData) DeleteTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, 0, 1);
					// connect DAQmx data and user interface callbackFn to all direct controls in the new panel 
					SetCtrlsInPanCBInfo(newDAQmxDev, DAQmxDevTaskSet_CB, newDAQmxDevPanHndl);
					// connect DAQmx data to the panel as well
					SetPanelAttribute(newDAQmxDevPanHndl, ATTR_CALLBACK_DATA, newDAQmxDev);
					// cleanup
					OKfree(newTCName);
					
					//------------------------------------------------------------------------------------------------
					// populate controls based on the device capabilities
					//------------------------------------------------------------------------------------------------
					
					// acquire
					if (ListNumItems(newDAQmxDev->attr->AIchan) || ListNumItems(newDAQmxDev->attr->DIlines) || 
						ListNumItems(newDAQmxDev->attr->DIports) || ListNumItems(newDAQmxDev->attr->CIchan)) 
						InsertListItem(newDAQmxDevPanHndl, TaskSetPan_IO, -1, "Acquire", DAQmxAcquire);  
					
					// generate
					if (ListNumItems(newDAQmxDev->attr->AOchan) || ListNumItems(newDAQmxDev->attr->DOlines) || 
						ListNumItems(newDAQmxDev->attr->DOports) || ListNumItems(newDAQmxDev->attr->COchan)) 
						InsertListItem(newDAQmxDevPanHndl, TaskSetPan_IO, -1, "Generate", DAQmxGenerate);  
					
					GetCtrlVal(newDAQmxDevPanHndl, TaskSetPan_IO, &ioVal);
					PopulateIOMode(newDAQmxDev, newDAQmxDevPanHndl, TaskSetPan_IOMode, ioVal);
					GetCtrlVal(newDAQmxDevPanHndl, TaskSetPan_IOMode, &ioMode);
					PopulateIOType(newDAQmxDev, newDAQmxDevPanHndl, TaskSetPan_IOType, ioVal, ioMode);
					
					//------------------------------------------------------------------------------------------------
					// add channels
					//------------------------------------------------------------------------------------------------
					PopulateChannels (newDAQmxDev);
					
					//------------------------------------------------------------------------------------------------
					// initialize Task Controller UI controls
					//------------------------------------------------------------------------------------------------
					// set repeats and change data type to size_t
					SetCtrlAttribute(newDAQmxDevPanHndl, TaskSetPan_Repeat, ATTR_DATA_TYPE, VAL_SIZE_T);
					SetCtrlVal(newDAQmxDevPanHndl, TaskSetPan_Repeat, GetTaskControlIterations(newDAQmxDev->taskController)); 
					// chage data type of iterations to size_t
					SetCtrlAttribute(newDAQmxDevPanHndl, TaskSetPan_TotalIterations, ATTR_DATA_TYPE, VAL_SIZE_T);
					// set wait
					SetCtrlVal(newDAQmxDevPanHndl, TaskSetPan_Wait, GetTaskControlIterationsWait(newDAQmxDev->taskController));
					// set mode
					SetCtrlVal(newDAQmxDevPanHndl, TaskSetPan_Mode, !GetTaskControlMode(newDAQmxDev->taskController));
					// configure Task Controller
					TaskControlEvent(newDAQmxDev->taskController, TASK_EVENT_CONFIGURE, NULL, NULL);
					
					// dim repeat if TC mode is continuous, otherwise undim
					if (GetTaskControlMode(newDAQmxDev->taskController) == TASK_FINITE) 
						// finite
						SetCtrlAttribute(newDAQmxDevPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, 0);
					else
						// continuous
						SetCtrlAttribute(newDAQmxDevPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, 1);
					
					// undim "Delete" menu item
					SetMenuBarAttribute(nidaq->menuBarHndl, nidaq->deleteDevMenuItemID, ATTR_DIMMED, 0);
					
					break;
					
				case DevListPan_DoneBTTN:
					
					HidePanel(panel);
					
					break;
			}

			break;
			
		case EVENT_LEFT_CLICK:
			
			// filter only left click event on the daq table
			if (control != DevListPan_DAQTable) return 0;
			
			Point Mouse_Coords;
			Point Cell;
			int columns;
			
			// Get number of table columns
			GetNumTableColumns (panel, control, &columns);
			
			Mouse_Coords.x = eventData2;   // column #
			Mouse_Coords.y = eventData1;   // row #
			GetTableCellFromPoint (panel, control, Mouse_Coords, &Cell);
			if ((currDev>0) && (Cell.x>0) && (Cell.y>0)) {
				// Return to normal previously highlighted cell
				SetTableCellRangeAttribute (panel, control, MakeRect(currDev,1,1,columns),ATTR_TEXT_BGCOLOR, VAL_WHITE);  
		    	// Set currently selected device from table
		    	currDev=Cell.y;
				// Highlight device
				SetTableCellRangeAttribute (panel, control, MakeRect(currDev,1,1,columns),ATTR_TEXT_BGCOLOR, VAL_YELLOW);
			}
			
			break;
	}
	return 0;
}

/*
void GetCO_HiLo(double freq,double dutycycle,double* hightime,double* lowtime)
{
	double period;
	if (freq!=0) {
		period=1/freq;
		*hightime=(period*dutycycle)/100;
		*lowtime=period-*hightime;
	}
	else {
		*hightime		= DAQmxDefault_CO_Frequency_Task_hightime;
		*lowtime		= DAQmxDefault_CO_Frequency_Task_lowtime;    
	}
} */


/// HIFN Breaks up a string into substrings separated by a token. Call first with idxstart = &string which will keep track of the string extraction process. 
/// HIFN Returned strings are allocated with malloc.
static char* substr(const char* token, char** idxstart)
{
	char* tmp = NULL;
	char* idxend;
	
	if (*idxstart == NULL) return NULL;
	
	idxend = strstr(*idxstart, token);
	if (idxend != NULL){
		if (**idxstart != 0) {
			tmp=malloc((idxend-*idxstart+1)*sizeof(char)); // allocate memory for characters to copy plus null character
			if (tmp == NULL) return NULL;
        	// Copy substring
			memcpy(tmp, *idxstart, (idxend-*idxstart)*sizeof(char));
			// Add null character
			*(tmp+(idxend-*idxstart)) = 0; 
			// Update
			*idxstart = idxend+strlen(token);
			return tmp;
		}
	} else {
		tmp = malloc((strlen(*idxstart)+1)*sizeof(char));
		if (tmp == NULL) return NULL;
		// Initialize with a null character
		*tmp = 0;
		// Copy substring
		strcpy(tmp, *idxstart);
		*idxstart = NULL; 
		return tmp;
	}
	return NULL;
}

static BOOL	ValidTaskControllerName	(char name[], void* dataPtr)
{
	return DLValidTaskControllerName(name);
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	NIDAQmxManager_type* 	nidaq	= (NIDAQmxManager_type*) mod;
	int						error	= 0;
	
	if (visibleFlag)
		errChk(DisplayPanel(nidaq->mainPanHndl));  
	else
		errChk(HidePanel(nidaq->mainPanHndl));
	
Error:
	return error;
}

static int AddDAQmxChannel (Dev_type* dev, DAQmxIO_type ioVal, DAQmxIOMode_type ioMode, int ioType, char chName[])
{
	char* newVChanName;
	
	switch (ioVal) {
		
		case DAQmxAcquire: 
			
			switch (ioMode) {
			
				case DAQmxAnalog:
					
					// try to use physical channel name for the VChan name, if it exists, then ask for another name
					newVChanName= StrDup(chName);
					if (DLVChanNameExists(newVChanName, NULL)) {
						OKfree(newVChanName);
						newVChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
						if (!newVChanName) return 0;	// action cancelled
					}
					
					//-------------------------------------------------
					// if there is no AI task then create it and add UI
					//-------------------------------------------------
							
					if(!dev->AITaskSet) {
						// init AI task structure data
						dev->AITaskSet = init_AIAOTaskSet_type();
								
						// load UI resources
						int AIAOTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AIAOTskSet);  
						// insert panel to UI and keep track of the AI task settings panel handle
						int newTabIdx = InsertPanelAsTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, AIAOTaskSetPanHndl); 
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, &dev->AITaskSet->panHndl);
						// change tab title to new Task Controller name
						SetTabPageAttribute(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, ATTR_LABEL_TEXT, DAQmxAITaskSetTabName);
					
						// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
						int 	panHndl;
						void*   callbackData;
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, &panHndl);
						GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
						if (!callbackData) DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, 1);
						// connect AI task settings data to the panel
						SetPanelAttribute(dev->AITaskSet->panHndl, ATTR_CALLBACK_DATA, dev->AITaskSet);
								
						//--------------------------
						// adjust "Channels" tab
						//--------------------------
						// get channels tab panel handle
						int chanPanHndl;
						GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_ChanTabIdx, &chanPanHndl);
						// remove "None" labelled channel tab
						DeleteTabPage(chanPanHndl, Chan_ChanSet, 0, 1);
						// add callback data and callback function to remove channels
						SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_FUNCTION_POINTER, RemoveDAQmxAIChannel_CB);
						SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_DATA, dev);
								
						//--------------------------
						// adjust "Settings" tab
						//--------------------------
						// get settings tab panel handle
						int setPanHndl;
						GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_SettingsTabIdx, &setPanHndl);
						// set controls to their default
						SetCtrlVal(setPanHndl, Set_SamplingRate, dev->AITaskSet->timing->sampleRate/1000);					// display in [kHz]
								
						SetCtrlAttribute(setPanHndl, Set_NSamples, ATTR_DATA_TYPE, VAL_SIZE_T);
						SetCtrlVal(setPanHndl, Set_NSamples, dev->AITaskSet->timing->nSamples);
							
						SetCtrlAttribute(setPanHndl, Set_BlockSize, ATTR_DATA_TYPE, VAL_SIZE_T);
						SetCtrlVal(setPanHndl, Set_BlockSize, dev->AITaskSet->timing->blockSize);
								
						SetCtrlVal(setPanHndl, Set_Duration, dev->AITaskSet->timing->nSamples/dev->AITaskSet->timing->sampleRate); 	// display in [s]
						// measurement mode
						InsertListItem(setPanHndl, Set_MeasMode, -1, "Finite", MeasFinite);
						InsertListItem(setPanHndl, Set_MeasMode, -1, "Continuous", MeasCont);
						int ctrlIdx;
						GetIndexFromValue(setPanHndl, Set_MeasMode, &ctrlIdx, dev->AITaskSet->timing->measMode); 
						SetCtrlIndex(setPanHndl, Set_MeasMode, ctrlIdx);
						// add callback to controls in the panel
						SetCtrlsInPanCBInfo(dev, AI_Settings_TaskSet_CB, setPanHndl);
								
						//--------------------------
						// adjust "Timing" tab
						//--------------------------
						// get timing tab panel handle
						int timingPanHndl;
						GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TimingTabIdx, &timingPanHndl);
								
						// make sure that the host controls are not dimmed before inserting terminal controls!
						NIDAQmx_NewTerminalCtrl(timingPanHndl, Timing_SampleClkSource, 0); // single terminal selection
						NIDAQmx_NewTerminalCtrl(timingPanHndl, Timing_RefClkSource, 0);    // single terminal selection
								
						// adjust sample clock terminal control properties
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
						// set default sample clock source
						dev->AITaskSet->timing->sampClkSource = StrDup("OnboardClock");
						SetCtrlVal(timingPanHndl, Timing_SampleClkSource, "OnboardClock");
					
						// adjust reference clock terminal control properties
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0);
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
						// set default reference clock source (none)
						SetCtrlVal(timingPanHndl, Timing_RefClkSource, "");
					
						// set ref clock freq and timeout to default
						SetCtrlVal(timingPanHndl, Timing_RefClkFreq, dev->AITaskSet->timing->refClkFreq/1e6);		// display in [MHz]						
						SetCtrlVal(timingPanHndl, Timing_Timeout, dev->AITaskSet->timeout);							// display in [s]
						
						// add callback to controls in the panel
						SetCtrlsInPanCBInfo(dev, AI_Timing_TaskSet_CB, timingPanHndl);
								
						//--------------------------
						// adjust "Trigger" tab
						//--------------------------
						// get trigger tab panel handle
						int trigPanHndl;
						GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TriggerTabIdx, &trigPanHndl);
								
						// remove "None" tab if there are supported triggers
						if (dev->attr->AITriggerUsage)
							DeleteTabPage(trigPanHndl, Trig_TrigSet, 0, 1);
								
						// add supported trigger panels
						// start trigger
						if (dev->attr->AITriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Start) {
							// load resources
							int start_DigEdgeTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig1);
							// add trigger data structure
							dev->AITaskSet->startTrig = init_TaskTrig_type(dev->AITaskSet->timing->refSampleRate);
									
							// add start trigger panel
							int newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Start");
							// get start trigger tab panel handle
							int startTrigPanHndl;
							GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &startTrigPanHndl);
							// add control to select trigger type and put background plate
							DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_Plate, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							int trigTypeCtrlID = DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_TrigType, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							// add callback data and callback function to the control
							SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, dev->AITaskSet->startTrig);
							SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskStartTrigType_CB);
							// insert trigger type options
							InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "None", Trig_None); 
							if (dev->attr->DigitalTriggering) {
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Digital Edge", Trig_DigitalEdge); 
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Digital Pattern", Trig_DigitalPattern);
							}
							
							if (dev->attr->AnalogTriggering) {
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Analog Edge", Trig_AnalogEdge); 
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Analog Window", Trig_AnalogWindow);
							}
							// set no trigger type by default
							SetCtrlIndex(startTrigPanHndl, trigTypeCtrlID, 0); 
									
							// discard resources
							DiscardPanel(start_DigEdgeTrig_PanHndl);
						}
								
						// reference trigger
						if (dev->attr->AITriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Reference) {
							// load resources
							int reference_DigEdgeTrig_PanHndl	= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig1);
							// add trigger data structure
							dev->AITaskSet->referenceTrig = init_TaskTrig_type(dev->AITaskSet->timing->refSampleRate);
									
							// add reference trigger panel
							int newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Reference");
							// get reference trigger tab panel handle
							int referenceTrigPanHndl;
							GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &referenceTrigPanHndl);
							// add control to select trigger type and put background plate
							DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_Plate, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							int trigTypeCtrlID = DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_TrigType, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							// add callback data and callback function to the control
							SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, dev->AITaskSet->referenceTrig);
							SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskReferenceTrigType_CB);
							// insert trigger type options
							InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "None", Trig_None); 
							if (dev->attr->DigitalTriggering) {
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Digital Edge", Trig_DigitalEdge); 
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Digital Pattern", Trig_DigitalPattern);
							}
									
							if (dev->attr->AnalogTriggering) {
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Analog Edge", Trig_AnalogEdge); 
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Analog Window", Trig_AnalogWindow);
							}
							// set no trigger type by default
							SetCtrlIndex(referenceTrigPanHndl, trigTypeCtrlID, 0); 
									
							// discard resources
							DiscardPanel(reference_DigEdgeTrig_PanHndl);
						}
								
						// pause trigger
						if (dev->attr->AITriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Pause) {
						}
							
						// advance trigger
						if (dev->attr->AITriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Advance) {
						}
								
						// arm start trigger
						if (dev->attr->AITriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_ArmStart) {
						}
								
						// handshake trigger
						if (dev->attr->AITriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Handshake) {
						}
								
					}
							
					//------------------------------------------------
					// add new channel
					//------------------------------------------------
					AIChannel_type* AIchanAttr 	= GetAIChannel(dev, chName);
					
					
					// mark channel as in use
					AIchanAttr->inUse = TRUE;
							
					int chanPanHndl;
					GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_ChanTabIdx, &chanPanHndl);
									
					switch (ioType) {
								
						//--------------------------------------
						// Voltage measurement
						//--------------------------------------
						case DAQmx_Val_Voltage:			
							
							// init new channel data
							ChanSet_AIAO_Voltage_type* newChan 	= (ChanSet_AIAO_Voltage_type*) init_ChanSet_AIAO_Voltage_type(chName, Chan_AI_Voltage, AIchanAttr, NULL);
							
							// insert new channel settings tab
							int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AIAOChSet);  
							int newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
							// change tab title
							char* shortChanName = strstr(chName, "/") + 1;  
							SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
							DiscardPanel(chanSetPanHndl); 
							chanSetPanHndl = 0;
							GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &newChan->baseClass.chanPanHndl);
							// add callbackdata to the channel panel
							SetPanelAttribute(newChan->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, newChan);
							// add callback data to the controls in the panel
							SetCtrlsInPanCBInfo(newChan, ChanSetAIAOVoltage_CB, newChan->baseClass.chanPanHndl);
							
							//--------------
							// terminal
							//--------------
							// select default terminal configuration from available choices in the following order
							if (AIchanAttr->terminalCfg | Terminal_Bit_RSE) newChan->terminal = Terminal_RSE;
							else
								if (AIchanAttr->terminalCfg | Terminal_Bit_NRSE) newChan->terminal = Terminal_NRSE;
								else
									if (AIchanAttr->terminalCfg | Terminal_Bit_Diff) newChan->terminal = Terminal_Diff;
									else
										if (AIchanAttr->terminalCfg | Terminal_Bit_PseudoDiff) newChan->terminal = Terminal_PseudoDiff;
									// if none of the above then Terminal_None is used which should not happen
							// populate terminal ring control with available terminal configurations
							if (AIchanAttr->terminalCfg | Terminal_Bit_RSE) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "RSE", Terminal_RSE);  
							if (AIchanAttr->terminalCfg | Terminal_Bit_NRSE) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "NRSE", Terminal_NRSE);  
							if (AIchanAttr->terminalCfg | Terminal_Bit_Diff) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "Diff", Terminal_Diff);  
							if (AIchanAttr->terminalCfg | Terminal_Bit_PseudoDiff) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "PseudoDiff", Terminal_PseudoDiff);
							// select terminal to match previously assigned value to newChan->terminal
							int terminalIdx;
							GetIndexFromValue(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, &terminalIdx, newChan->terminal); 
							SetCtrlIndex(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, terminalIdx);
							
							//--------------
							// AI ranges
							//--------------
							// insert AI ranges and make largest range default
							char label[20];
							for (int i = 0; i < AIchanAttr->Vrngs->Nrngs ; i++){
								sprintf(label, "%.2f, %.2f", AIchanAttr->Vrngs->low[i], AIchanAttr->Vrngs->high[i]);
								InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Range, -1, label, i);
							}
							
							SetCtrlIndex(newChan->baseClass.chanPanHndl, AIAOChSet_Range, AIchanAttr->Vrngs->Nrngs - 1);
							newChan->Vmax = AIchanAttr->Vrngs->high[AIchanAttr->Vrngs->Nrngs - 1];
							newChan->Vmin = AIchanAttr->Vrngs->low[AIchanAttr->Vrngs->Nrngs - 1];
							
							//--------------------------
							// Create and register VChan
							//--------------------------
							
							newChan->baseClass.srcVChan = init_SourceVChan_type(newVChanName, DL_Waveform_Double, newChan, VChanConnected, VChanDisconnected);  
							DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newChan->baseClass.srcVChan);
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_VChanName, newVChanName);
							OKfree(newVChanName);
							
							//---------------------------------------
							// Add new AI channel to list of channels
							//---------------------------------------
							ListInsertItem(dev->AITaskSet->chanSet, &newChan, END_OF_LIST);
							
							 break;
							//--------------------------------------  
							// add here more channel cases
							//--------------------------------------  
								
					}
					break;
							
			
				case DAQmxDigital:
			
					break;
			
				case DAQmxCounter:
						   			// try to use physical channel name for the VChan name, if it exists, then ask for another name
						newVChanName= StrDup(chName);
						if (DLVChanNameExists(newVChanName, NULL)) {
							OKfree(newVChanName);
							newVChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
							if (!newVChanName) return 0;	// action cancelled
						}
					
						//-------------------------------------------------
						// if there is no CI task then create it and add UI
						//-------------------------------------------------
							
						if(!dev->CITaskSet) {
							// init CO task structure data
							dev->CITaskSet = (CITaskSet_type*) init_CITaskSet_type();    
								
					
							 
							
							// load UI resources
							int CICOTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOTskSet);  
							// insert panel to UI and keep track of the AI task settings panel handle
							int newTabIdx = InsertPanelAsTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, CICOTaskSetPanHndl); 
							GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, &dev->CITaskSet->panHndl);
							// change tab title to new Task Controller name
							SetTabPageAttribute(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, ATTR_LABEL_TEXT, DAQmxCITaskSetTabName);
							
								// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
							int 	panHndl;
							void*   callbackData;
							GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, &panHndl);
							GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
							if (!callbackData) DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, 1);
							// connect AI task settings data to the panel
							SetPanelAttribute(dev->CITaskSet->panHndl, ATTR_CALLBACK_DATA, dev->CITaskSet);
								
								//--------------------------
							// adjust "Channels" tab
							//--------------------------
							// get channels tab panel handle
							int chanPanHndl;
					//		GetPanelHandleFromTabPage(dev->CITaskSet->panHndl, CICOTskSet_Tab, DAQmxCICOTskSet_ChanTabIdx, &chanPanHndl);
							// remove "None" labelled channel tab
					//		DeleteTabPage(chanPanHndl, Chan_ChanSet, 0, 1);
							// add callback data and callback function to remove channels
					//		SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_FUNCTION_POINTER, RemoveDAQmxCIChannel_CB);
					//		SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_DATA, dev);
								
							//------------------------------------------------
							// add new channel
							//------------------------------------------------
					
							CIChannel_type* CIchanAttr 	= GetCIChannel(dev, chName);
				
							// mark channel as in use
							CIchanAttr->inUse = TRUE;
							
					//		GetPanelHandleFromTabPage(dev->CITaskSet->panHndl, CICOTskSet_Tab, DAQmxCICOTskSet_ChanTabIdx, &chanPanHndl);
							
							CIEdgeChanSet_type* newChan 	=  (CIEdgeChanSet_type*) init_ChanSet_CI_Frequency_type(chName);
							
							// insert new channel settings tab
							int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOChSet);  
							newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
							// change tab title
							char* shortChanName = strstr(chName, "/") + 1;  
							SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
							DiscardPanel(chanSetPanHndl); 
							chanSetPanHndl = 0;
							GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &newChan->baseClass.chanPanHndl);
							// add callbackdata to the channel panel
							
							SetPanelAttribute(newChan->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, newChan);
							// add callback data to the controls in the panel
						//	SetCtrlsInPanCBInfo(newChan, ChanSetAIAOVoltage_CB, newChan->baseClass.chanPanHndl);
						
							//--------------------------
							// Create and register VChan
							//--------------------------
							
							newChan->baseClass.srcVChan = init_SourceVChan_type(newVChanName, DL_Waveform_Double, newChan, VChanConnected, VChanDisconnected);  
							DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newChan->baseClass.srcVChan);
							SetCtrlVal(newChan->baseClass.chanPanHndl, SETPAN_VChanName, newVChanName);
							OKfree(newVChanName);
							
							//---------------------------------------
							// Add new CI channel to list of channels
							//---------------------------------------
							ListInsertItem(dev->CITaskSet->chanTaskSet, &newChan, END_OF_LIST);
			
						}
					break;
			
				case DAQmxTEDS:
			
					break;
			}
			break;
			
		case DAQmxGenerate:
			
			switch (ioMode) {
			
				case DAQmxAnalog:
					
					// try to use physical channel name for the VChan name, if it exists, then ask for another name
					newVChanName = StrDup(chName);
					if (DLVChanNameExists(newVChanName, NULL)) {
						OKfree(newVChanName);
						newVChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
						if (!newVChanName) return 0;	// action cancelled
					}
					
					//-------------------------------------------------
					// if there is no AO task then create it and add UI
					//-------------------------------------------------
							
					if(!dev->AOTaskSet) {
						// init AO task structure data
						dev->AOTaskSet = init_AIAOTaskSet_type();
								
						// load UI resources
						int AIAOTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AIAOTskSet);  
						// insert panel to UI and keep track of the AO task settings panel handle
						int newTabIdx = InsertPanelAsTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, AIAOTaskSetPanHndl); 
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, &dev->AOTaskSet->panHndl);
						// change tab title to new Task Controller name
						SetTabPageAttribute(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, ATTR_LABEL_TEXT, DAQmxAOTaskSetTabName);
					
						// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
						int 	panHndl;
						void*   callbackData;
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, &panHndl);
						GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
						if (!callbackData) DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, 1);
						// connect AO task settings data to the panel
						SetPanelAttribute(dev->AOTaskSet->panHndl, ATTR_CALLBACK_DATA, dev->AOTaskSet);
								
						//--------------------------
						// adjust "Channels" tab
						//--------------------------
						// get channels tab panel handle
						int chanPanHndl;
						GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_ChanTabIdx, &chanPanHndl);
						// remove "None" labelled channel tab
						DeleteTabPage(chanPanHndl, Chan_ChanSet, 0, 1);
						// add callback data and callback function to remove channels
						SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_FUNCTION_POINTER, RemoveDAQmxAOChannel_CB);
						SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_DATA, dev);
								
								
						//--------------------------
						// adjust "Settings" tab
						//--------------------------
						// get settings tab panel handle
						int setPanHndl;
						GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_SettingsTabIdx, &setPanHndl);
						// set controls to their default
						SetCtrlVal(setPanHndl, Set_SamplingRate, dev->AOTaskSet->timing->sampleRate / 1000);					// display in [kHz]
								
						SetCtrlAttribute(setPanHndl, Set_NSamples, ATTR_DATA_TYPE, VAL_SIZE_T);
						SetCtrlVal(setPanHndl, Set_NSamples, dev->AOTaskSet->timing->nSamples);
								
						SetCtrlAttribute(setPanHndl, Set_BlockSize, ATTR_DATA_TYPE, VAL_SIZE_T);
						SetCtrlVal(setPanHndl, Set_BlockSize, dev->AOTaskSet->timing->blockSize);
							
						SetCtrlVal(setPanHndl, Set_Duration, dev->AOTaskSet->timing->nSamples / dev->AOTaskSet->timing->sampleRate); 	// display in [s]
						// measurement mode
						InsertListItem(setPanHndl, Set_MeasMode, -1, "Finite", MeasFinite);
						InsertListItem(setPanHndl, Set_MeasMode, -1, "Continuous", MeasCont);
						int ctrlIdx;    
						GetIndexFromValue(setPanHndl, Set_MeasMode, &ctrlIdx, dev->AOTaskSet->timing->measMode);
						SetCtrlIndex(setPanHndl, Set_MeasMode, ctrlIdx);
						// add callback to controls in the panel
						SetCtrlsInPanCBInfo(dev, AO_Settings_TaskSet_CB, setPanHndl);
								
						//--------------------------
						// adjust "Timing" tab
						//--------------------------
						// get timing tab panel handle
						int timingPanHndl;
						GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TimingTabIdx, &timingPanHndl);
								
						// make sure that the host controls are not dimmed before inserting terminal controls!
						NIDAQmx_NewTerminalCtrl(timingPanHndl, Timing_SampleClkSource, 0); // single terminal selection
						NIDAQmx_NewTerminalCtrl(timingPanHndl, Timing_RefClkSource, 0);    // single terminal selection
								
						// adjust sample clock terminal control properties
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
						// set default sample clock source
						dev->AOTaskSet->timing->sampClkSource = StrDup("OnboardClock");
						SetCtrlVal(timingPanHndl, Timing_SampleClkSource, "OnboardClock");
					
						// adjust reference clock terminal control properties
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0);
						NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
						// set default reference clock source (none)
						SetCtrlVal(timingPanHndl, Timing_RefClkSource, "");
					
						// set ref clock freq and timeout to default
						SetCtrlVal(timingPanHndl, Timing_RefClkFreq, dev->AOTaskSet->timing->refClkFreq / 1e6);		// display in [MHz]						
						SetCtrlVal(timingPanHndl, Timing_Timeout, dev->AOTaskSet->timeout);							// display in [s]
						// add callback to controls in the panel
						SetCtrlsInPanCBInfo(dev, AO_Timing_TaskSet_CB, timingPanHndl);
								
						//--------------------------
						// adjust "Trigger" tab
						//--------------------------
						// get trigger tab panel handle
						int trigPanHndl;
						GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TriggerTabIdx, &trigPanHndl);
								
						// remove "None" tab if there are supported triggers
						if (dev->attr->AOTriggerUsage)
							DeleteTabPage(trigPanHndl, Trig_TrigSet, 0, 1);
							
						// add supported trigger panels
						// start trigger
						if (dev->attr->AOTriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Start) {
							// load resources
							int start_DigEdgeTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig1);
							// add trigger data structure
							dev->AOTaskSet->startTrig = init_TaskTrig_type(dev->AOTaskSet->timing->refSampleRate);
									
							// add start trigger panel
							int newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Start");
							// get start trigger tab panel handle
							int startTrigPanHndl;
							GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &startTrigPanHndl);
							// add control to select trigger type and put background plate
							DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_Plate, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							int trigTypeCtrlID = DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_TrigType, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							// add callback data and callback function to the control
							SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, dev->AOTaskSet->startTrig);
							SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskStartTrigType_CB);
							// insert trigger type options
							InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "None", Trig_None); 
							if (dev->attr->DigitalTriggering) {
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Digital Edge", Trig_DigitalEdge); 
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Digital Pattern", Trig_DigitalPattern);
							}
									
							if (dev->attr->AnalogTriggering) {
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Analog Edge", Trig_AnalogEdge); 
								InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Analog Window", Trig_AnalogWindow);
							}
							// set no trigger type by default
							SetCtrlIndex(startTrigPanHndl, trigTypeCtrlID, 0); 
									
							// discard resources
							DiscardPanel(start_DigEdgeTrig_PanHndl);
						}
								
						// reference trigger
						if (dev->attr->AOTriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Reference) {
							// load resources
							int reference_DigEdgeTrig_PanHndl	= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig1);
							// add trigger data structure
							dev->AOTaskSet->referenceTrig = init_TaskTrig_type(dev->AOTaskSet->timing->refSampleRate);
									
							// add reference trigger panel
							int newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Reference");
							// get reference trigger tab panel handle
							int referenceTrigPanHndl;
							GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &referenceTrigPanHndl);
							// add control to select trigger type
							DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_Plate, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							int trigTypeCtrlID = DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_TrigType, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
							// add callback data and callback function to the control
							SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, dev->AOTaskSet->referenceTrig);
							SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskReferenceTrigType_CB);
							// insert trigger type options
							InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "None", Trig_None); 
							if (dev->attr->DigitalTriggering) {
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Digital Edge", Trig_DigitalEdge); 
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Digital Pattern", Trig_DigitalPattern);
							}
							
							if (dev->attr->AnalogTriggering) {
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Analog Edge", Trig_AnalogEdge); 
								InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Analog Window", Trig_AnalogWindow);
							}
							// set no trigger type by default
							SetCtrlIndex(referenceTrigPanHndl, trigTypeCtrlID, 0); 
							
							// discard resources
							DiscardPanel(reference_DigEdgeTrig_PanHndl);
						}
								
						// pause trigger
						if (dev->attr->AOTriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Pause) {
						}
								
						// advance trigger
						if (dev->attr->AOTriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Advance) {
						}
								
						// arm start trigger
						if (dev->attr->AOTriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_ArmStart) {
						}
								
						// handshake trigger
						if (dev->attr->AOTriggerUsage | DAQmx_Val_Bit_TriggerUsageTypes_Handshake) {
						}
								
					}
							
					//------------------------------------------------
					// add new channel
					//------------------------------------------------
					
					AOChannel_type* AOchanAttr 	= GetAOChannel(dev, chName);
				
					// mark channel as in use
					AOchanAttr->inUse = TRUE;
							
					int chanPanHndl;
					GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_ChanTabIdx, &chanPanHndl);
					
					switch (ioType) {
						
						//--------------------------------------
						// Voltage measurement
						//--------------------------------------
						case DAQmx_Val_Voltage:
							
							// init new channel data
							ChanSet_AIAO_Voltage_type* newChan 	= (ChanSet_AIAO_Voltage_type*) init_ChanSet_AIAO_Voltage_type(chName, Chan_AO_Voltage, NULL, AOchanAttr);
							
							// insert new channel settings tab
							int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AIAOChSet);  
							int newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
							// change tab title
							char* shortChanName = strstr(chName, "/") + 1;  
							SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
							DiscardPanel(chanSetPanHndl); 
							chanSetPanHndl = 0;
							GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &newChan->baseClass.chanPanHndl);
							// add callbackdata to the channel panel
							SetPanelAttribute(newChan->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, newChan);
							// add callback data to the controls in the panel
							SetCtrlsInPanCBInfo(newChan, ChanSetAIAOVoltage_CB, newChan->baseClass.chanPanHndl);
							
							//--------------
							// terminal
							//--------------
							// select default terminal configuration from available choices in the following order
							if (AOchanAttr->terminalCfg | Terminal_Bit_RSE) newChan->terminal = Terminal_RSE;
							else
								if (AOchanAttr->terminalCfg | Terminal_Bit_NRSE) newChan->terminal = Terminal_NRSE;
								else
									if (AOchanAttr->terminalCfg | Terminal_Bit_Diff) newChan->terminal = Terminal_Diff;
									else
										if (AOchanAttr->terminalCfg | Terminal_Bit_PseudoDiff) newChan->terminal = Terminal_PseudoDiff;
											// if none of the above then Terminal_None is used which should not happen
							// populate terminal ring control with available terminal configurations
							if (AOchanAttr->terminalCfg | Terminal_Bit_RSE) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "RSE", Terminal_RSE);  
							if (AOchanAttr->terminalCfg | Terminal_Bit_NRSE) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "NRSE", Terminal_NRSE);  
							if (AOchanAttr->terminalCfg | Terminal_Bit_Diff) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "Diff", Terminal_Diff);  
							if (AOchanAttr->terminalCfg | Terminal_Bit_PseudoDiff) InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, -1, "PseudoDiff", Terminal_PseudoDiff);
							// select terminal to match previously assigned value to newChan->terminal
							int terminalIdx;
							GetIndexFromValue(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, &terminalIdx, newChan->terminal); 
							SetCtrlIndex(newChan->baseClass.chanPanHndl, AIAOChSet_Terminal, terminalIdx);
							
							//--------------
							// AO ranges
							//--------------
							// insert AO ranges and make largest range default
							char label[20];
							for (int i = 0; i < AOchanAttr->Vrngs->Nrngs ; i++){
								sprintf(label, "%.2f, %.2f", AOchanAttr->Vrngs->low[i], AOchanAttr->Vrngs->high[i]);
								InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_Range, -1, label, i);
							}
							
							SetCtrlIndex(newChan->baseClass.chanPanHndl, AIAOChSet_Range, AOchanAttr->Vrngs->Nrngs - 1);
							newChan->Vmax = AOchanAttr->Vrngs->high[AOchanAttr->Vrngs->Nrngs - 1];
							newChan->Vmin = AOchanAttr->Vrngs->low[AOchanAttr->Vrngs->Nrngs - 1];
							
							//-------------------------------------------------------------
							// Create and register VChan with Task Controller and framework
							//-------------------------------------------------------------
							
							DLDataTypes allowedPacketTypes[] = {DL_Waveform_Double, DL_RepeatedWaveform_Double};
							
							newChan->baseClass.sinkVChan = init_SinkVChan_type(newVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), newChan, VChanConnected, VChanDisconnected);  
							DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newChan->baseClass.sinkVChan);
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_VChanName, newVChanName);
							AddSinkVChan(dev->taskController, newChan->baseClass.sinkVChan, DataReceivedTC, TASK_VCHAN_FUNC_ITERATE); 
							OKfree(newVChanName);
							
							//---------------------------------------
							// Add new AO channel to list of channels
							//---------------------------------------
							ListInsertItem(dev->AOTaskSet->chanSet, &newChan, END_OF_LIST);
							
							break;
							//--------------------------------------
							// add here more channel types
							//--------------------------------------
					}
					break;
			
				case DAQmxDigital:
			
					break;
			
				case DAQmxCounter:
							   			// try to use physical channel name for the VChan name, if it exists, then ask for another name
						newVChanName= StrDup(chName);
						if (DLVChanNameExists(newVChanName, NULL)) {
							OKfree(newVChanName);
							newVChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
							if (!newVChanName) return 0;	// action cancelled
						}
					
						//-------------------------------------------------
						// if there is no CO task then create it and add UI
						//-------------------------------------------------
							
						if(!dev->COTaskSet) {
							// init CO task structure data
							dev->COTaskSet = (COTaskSet_type*) init_COTaskSet_type();    
							
							// load UI resources
							int CICOTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOTskSet);  
							// insert panel to UI and keep track of the AI task settings panel handle
							int newTabIdx = InsertPanelAsTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, CICOTaskSetPanHndl); 
							GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, &dev->COTaskSet->panHndl);
							// change tab title to new Task Controller name
							SetTabPageAttribute(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, ATTR_LABEL_TEXT, DAQmxCOTaskSetTabName);
				
								// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
							int 	panHndl;
							void*   callbackData;
							GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, &panHndl);
							GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
							if (!callbackData) DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, 1);
							// connect CO task settings data to the panel
							SetPanelAttribute(dev->COTaskSet->panHndl, ATTR_CALLBACK_DATA, dev->COTaskSet);
							
								// add callback data and callback function to remove channels
							SetCtrlAttribute(dev->COTaskSet->panHndl, Chan_ChanSet,ATTR_CALLBACK_FUNCTION_POINTER, RemoveDAQmxCOChannel_CB);
							SetCtrlAttribute(dev->COTaskSet->panHndl, Chan_ChanSet,ATTR_CALLBACK_DATA, dev);
							
								
						}
						
							int 	panHndl;
							void*   callbackData; 
							// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
							GetPanelHandleFromTabPage(dev->COTaskSet->panHndl, Chan_ChanSet, 0, &panHndl);
							GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
							if (!callbackData) DeleteTabPage(dev->COTaskSet->panHndl, Chan_ChanSet, 0, 1);
			
							//------------------------------------------------
							// add new channel
							//------------------------------------------------
					
							COChannel_type* COchanAttr 	= GetCOChannel(dev, chName);
				
							// mark channel as in use
							COchanAttr->inUse = TRUE;
							
						//	GetPanelHandleFromTabPage(dev->COTaskSet->panHndl, CICOTskSet_Tab, DAQmxCICOTskSet_ChanTabIdx, &chanPanHndl);
							int ioType;
							Channel_type chanType;
							GetCtrlVal(dev->devPanHndl, TaskSetPan_IOType, &ioType);  
							//set channel type 
							switch (ioType){
								case DAQmx_Val_Pulse_Freq:
									chanType=Chan_CO_Pulse_Frequency;
								break;
								case DAQmx_Val_Pulse_Time:
									chanType=Chan_CO_Pulse_Time;	
								break;
								case DAQmx_Val_Pulse_Ticks:
									chanType=Chan_CO_Pulse_Ticks;	
								break;
							}
							ChanSet_CO_type* newChan 	=  (ChanSet_CO_type*) init_ChanSet_CO_type(chName,chanType);
							
							// insert new channel tab
							int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOChSet);  
							int newTabIdx = InsertPanelAsTabPage(dev->COTaskSet->panHndl, Chan_ChanSet, -1, chanSetPanHndl); 
							// change tab title
							char* shortChanName = strstr(chName, "/") + 1;  
							SetTabPageAttribute(dev->COTaskSet->panHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
							DiscardPanel(chanSetPanHndl); 
							chanSetPanHndl = 0;
							GetPanelHandleFromTabPage(dev->COTaskSet->panHndl, Chan_ChanSet, newTabIdx, &newChan->baseClass.chanPanHndl);
							
							// add callbackdata to the channel panel
							SetPanelAttribute(newChan->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, newChan);
							
						
							// add callback data to the controls in the panel
							//SetCtrlsInPanCBInfo(newChan, ChanSetCO_CB, newChan->baseClass.chanPanHndl);
						
						//--------------------------
						// adjust "Settings" tab
						//--------------------------
						int settingsPanHndl;
						GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_SettingsTabIdx, &settingsPanHndl);
						GetCtrlVal(settingsPanHndl,SETPAN_Timeout,&dev->COTaskSet->timeout);
						
						// add callback to controls in the panel
						SetCtrlsInPanCBInfo(dev, ChanSetCO_CB, settingsPanHndl);   
								
						//--------------------------
						// adjust "Timing" tab
						//--------------------------
						// get timing tab panel handle
						int timingPanHndl;
						
						GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl);
						
						int CO_TimPanHndl;
						
						double time_in_ms;
						
						switch (chanType){
								case Chan_CO_Pulse_Frequency:
									CO_TimPanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, COFREQPAN); 
									newChan->timing->freqCtrlID = DuplicateCtrl(CO_TimPanHndl, COFREQPAN_Frequency, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->freqCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->freqCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_Freq_TaskSet_CB);
									
									newChan->timing->dutycycleCtrlID = DuplicateCtrl(CO_TimPanHndl, COFREQPAN_DutyCycle, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->dutycycleCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->dutycycleCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_Duty_TaskSet_CB);
									
									newChan->timing->initdelayCtrlID = DuplicateCtrl(CO_TimPanHndl, COFREQPAN_InitialDelay, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->initdelayCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->initdelayCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_InitDelay_TaskSet_CB);
									
									newChan->timing->idlestateCtrlID = DuplicateCtrl(CO_TimPanHndl, COFREQPAN_IdleState, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->idlestateCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->idlestateCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_IdleState_TaskSet_CB);
									DuplicateCtrl(timingPanHndl, COFREQPAN_Plate, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);   
									DiscardPanel(CO_TimPanHndl);
									
									
									GetCtrlVal(timingPanHndl,newChan->timing->freqCtrlID,&newChan->frequency);  
									GetCtrlVal(timingPanHndl,newChan->timing->dutycycleCtrlID,&newChan->dutycycle);
									newChan->units=DAQmx_Val_Hz;
									break;
									
								case Chan_CO_Pulse_Time:
									CO_TimPanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, COTIMPAN); 
									newChan->timing->timehighCtrlID = DuplicateCtrl(CO_TimPanHndl, COTIMPAN_HighTime, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->timehighCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->timehighCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_THigh_TaskSet_CB);
									//set default timing values  
									time_in_ms=newChan->hightime*1000;
									SetCtrlVal(timingPanHndl,newChan->timing->timehighCtrlID,time_in_ms);
									
									newChan->timing->timelowCtrlID = DuplicateCtrl(CO_TimPanHndl, COTIMPAN_LowTime, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->timelowCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->timelowCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_TLow_TaskSet_CB);
									//set default timing values  
									time_in_ms=newChan->lowtime*1000; 
									SetCtrlVal(timingPanHndl,newChan->timing->timelowCtrlID,time_in_ms);
									
									newChan->timing->initdelayCtrlID = DuplicateCtrl(CO_TimPanHndl, COTIMPAN_InitialDelay, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->initdelayCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->initdelayCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_InitDelay_TaskSet_CB);
									
									newChan->timing->idlestateCtrlID = DuplicateCtrl(CO_TimPanHndl, COTIMPAN_IdleState, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->idlestateCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->idlestateCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_IdleState_TaskSet_CB);
									DuplicateCtrl(timingPanHndl, COTIMPAN_Plate, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
									DiscardPanel(CO_TimPanHndl);
									
									
									
									
									newChan->units=DAQmx_Val_Seconds;
									break;
									
								case Chan_CO_Pulse_Ticks:
									CO_TimPanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, COTICKPAN); 
									newChan->timing->tickshighCtrlID = DuplicateCtrl(CO_TimPanHndl, COTICKPAN_HighTicks, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->tickshighCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->tickshighCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_TicksHigh_TaskSet_CB);
									
									newChan->timing->tickslowCtrlID = DuplicateCtrl(CO_TimPanHndl, COTICKPAN_LowTicks, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->tickslowCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->tickslowCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_TicksLow_TaskSet_CB);
									
									newChan->timing->initdelayCtrlID = DuplicateCtrl(CO_TimPanHndl, COTICKPAN_InitialDelay, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->initdelayCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->initdelayCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_InitDelay_TaskSet_CB);
									
									newChan->timing->idlestateCtrlID = DuplicateCtrl(CO_TimPanHndl, COTICKPAN_IdleState, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
									SetCtrlAttribute(timingPanHndl, newChan->timing->idlestateCtrlID, ATTR_CALLBACK_DATA, newChan);
									SetCtrlAttribute(timingPanHndl, newChan->timing->idlestateCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, CO_IdleState_TaskSet_CB);
									DuplicateCtrl(timingPanHndl, COTICKPAN_Plate, timingPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);    
									DiscardPanel(CO_TimPanHndl);
									
									
									GetCtrlVal(timingPanHndl,newChan->timing->tickshighCtrlID,&newChan->highticks);  
									GetCtrlVal(timingPanHndl,newChan->timing->tickslowCtrlID,&newChan->lowticks);
									newChan->units=DAQmx_Val_Ticks;		//not needed
									break;
							}

					
					
						
							//--------------------------
						// adjust "Clk" tab
						//--------------------------
						// get timing tab panel handle
						
						// add trigger data structure
						
						newChan->baseClass.referenceTrig = init_TaskTrig_type(0);	   //? lex
						
						int clkPanHndl;
						GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_ClkTabIdx, &clkPanHndl);
								
						// make sure that the host controls are not dimmed before inserting terminal controls!
						NIDAQmx_NewTerminalCtrl(clkPanHndl, CLKPAN_RefClkSource, 0); // single terminal selection
					
								
						// adjust sample clock terminal control properties
						NIDAQmx_SetTerminalCtrlAttribute(clkPanHndl, CLKPAN_RefClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
						NIDAQmx_SetTerminalCtrlAttribute(clkPanHndl, CLKPAN_RefClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
						
						InsertListItem(clkPanHndl,CLKPAN_RefClockSlope , 0, "Rising", TrigSlope_Rising); 
						InsertListItem(clkPanHndl,CLKPAN_RefClockSlope , 1, "Falling", TrigSlope_Falling);
						newChan->baseClass.referenceTrig->slope=TrigSlope_Rising;
						
						InsertListItem(clkPanHndl, CLKPAN_RefClockType, -1, "None", Trig_None); 
						InsertListItem(clkPanHndl, CLKPAN_RefClockType, -1, "Digital Edge", Trig_DigitalEdge); 
						newChan->baseClass.referenceTrig->trigType=Trig_None;
						SetCtrlAttribute(clkPanHndl,CLKPAN_RefClockSlope,ATTR_DIMMED,TRUE);
						SetCtrlAttribute(clkPanHndl,CLKPAN_RefClkSource,ATTR_DIMMED,TRUE); 
						
						
						// set default sample clock source
						newChan->baseClass.referenceTrig->trigSource = StrDup("OnboardClock");
						SetCtrlVal(clkPanHndl, CLKPAN_RefClkSource, "OnboardClock");
						
						
					
						
						
			
						// add callback to controls in the panel
						SetCtrlsInPanCBInfo(newChan, CO_Clk_TaskSet_CB, clkPanHndl);
						
						//--------------------------
						// adjust "Trigger" tab
						//--------------------------
						// get trigger tab panel handle
						newChan->baseClass.startTrig = init_TaskTrig_type(0);  //? lex  
						
						int trigPanHndl;
						GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TriggerTabIdx, &trigPanHndl);
								
							// make sure that the host controls are not dimmed before inserting terminal controls!
						NIDAQmx_NewTerminalCtrl(trigPanHndl, TRIGPAN_Source, 0); // single terminal selection
					
								
						// adjust sample clock terminal control properties
						NIDAQmx_SetTerminalCtrlAttribute(trigPanHndl, TRIGPAN_Source, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
						NIDAQmx_SetTerminalCtrlAttribute(trigPanHndl, TRIGPAN_Source, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
						
						// set default start trigger source
						newChan->baseClass.startTrig->trigSource = StrDup("/Dev2/ao/StartTrigger");
						SetCtrlVal(trigPanHndl, TRIGPAN_Source, "/Dev2/ao/StartTrigger");
						
						
						// insert trigger type options
						InsertListItem(trigPanHndl, TRIGPAN_TrigType, -1, "None", Trig_None); 
						InsertListItem(trigPanHndl, TRIGPAN_TrigType, -1, "Digital Edge", Trig_DigitalEdge); 
						newChan->baseClass.startTrig->trigType=Trig_None;
						SetCtrlAttribute(trigPanHndl,TRIGPAN_Slope,ATTR_DIMMED,TRUE);
						SetCtrlAttribute(trigPanHndl,TRIGPAN_Source,ATTR_DIMMED,TRUE); 									   ;  
						
						InsertListItem(trigPanHndl,TRIGPAN_Slope , 0, "Rising", TrigSlope_Rising); 
						InsertListItem(trigPanHndl,TRIGPAN_Slope , 1, "Falling", TrigSlope_Falling);
						newChan->baseClass.startTrig->slope=TrigSlope_Rising;
						
							// measurement mode
						InsertListItem(trigPanHndl, TRIGPAN_MeasMode, -1, "Finite", MeasFinite);
						InsertListItem(trigPanHndl, TRIGPAN_MeasMode, -1, "Continuous", MeasCont);
						int ctrlIdx;
						GetIndexFromValue(trigPanHndl, TRIGPAN_MeasMode, &ctrlIdx, newChan->timing->measMode ); 
						
						SetCtrlIndex(trigPanHndl, TRIGPAN_MeasMode, ctrlIdx);
						
						
						GetCtrlVal(trigPanHndl,TRIGPAN_NSamples,&newChan->timing->nSamples); 
			
						// add callback to controls in the panel
						SetCtrlsInPanCBInfo(newChan, CO_Trig_TaskSet_CB, trigPanHndl);
						
							//end insert	
						
						
						
								//--------------------------
							// Create and register VChan
							//--------------------------
							int settingspanel;
							GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB,DAQmxCICOTskSet_SettingsTabIdx , &settingspanel);    
							
							newChan->baseClass.srcVChan = init_SourceVChan_type(newVChanName, DL_Waveform_Double, newChan, VChanConnected, VChanDisconnected);  
							DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newChan->baseClass.srcVChan);
							SetCtrlVal(settingspanel, SETPAN_VChanName, newVChanName);
							OKfree(newVChanName);
							
							//---------------------------------------
							// Add new CO channel to list of channels
							//---------------------------------------
							ListInsertItem(dev->COTaskSet->chanTaskSet, &newChan, END_OF_LIST);
					
								
					}	
			
					break;
			
				case DAQmxTEDS:
			
					break;
			}
	
	return 0;		
}
				  
static void DisplayLastDAQmxLibraryError (void)
{
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	if (!errMsg) {
		DLMsg("Error: Out of memory.\n\n", 1);
		return;
	}
	
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	DLMsg(errMsg, 1);
	DLMsg("\n\n", 0);
	free(errMsg);
}

static int ClearDAQmxTasks (Dev_type* dev)
{
	int error        = 0;  
	
	if (!dev) return 0;	// do nothing
	
	// AI task
	if (dev->AITaskSet)
	if (dev->AITaskSet->taskHndl) {
		errChk(DAQmxClearTask(dev->AITaskSet->taskHndl));
		dev->AITaskSet->taskHndl = NULL;
	}
	
	// AO task
	if (dev->AOTaskSet)
	if (dev->AOTaskSet->taskHndl) {
		errChk(DAQmxClearTask(dev->AOTaskSet->taskHndl));
		dev->AOTaskSet->taskHndl = NULL;
	}
	
	// DI task
	if (dev->DITaskSet)
	if (dev->DITaskSet->taskHndl) {
		errChk(DAQmxClearTask(dev->DITaskSet->taskHndl));
		dev->DITaskSet->taskHndl = NULL;
	}
	
	// DO task
	if (dev->DOTaskSet)
	if (dev->DOTaskSet->taskHndl) {
		errChk(DAQmxClearTask(dev->DOTaskSet->taskHndl));
		dev->DOTaskSet->taskHndl = NULL;
	}
	
	// CI task
	if (dev->CITaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems			= ListNumItems(dev->CITaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nItems; i++) {	
			chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if ((*chanSetPtrPtr)->taskHndl) {
				DAQmxClearTask((*chanSetPtrPtr)->taskHndl);
				(*chanSetPtrPtr)->taskHndl = NULL;
			}
		}
	}
	
	// CO task
	if (dev->COTaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems			= ListNumItems(dev->COTaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nItems; i++) {	
			chanSetPtrPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);	
			if ((*chanSetPtrPtr)->taskHndl) {
				DAQmxClearTask((*chanSetPtrPtr)->taskHndl);
				(*chanSetPtrPtr)->taskHndl = NULL;
			}
		}
	}
	
	return 0;
	
Error:
	DisplayLastDAQmxLibraryError();
	return error;
}

static FCallReturn_type* StopDAQmxTasks (Dev_type* dev)
{
#define StopDAQmxTasks_Err_NULLDev		-1
	
	int 				error			= 0;  
	FCallReturn_type*   fCallReturn		= NULL;
	
	if (!dev) return init_FCallReturn_type(StopDAQmxTasks_Err_NULLDev, "StopDAQmxTasks", "No reference to device");  
	
	// AI task
	if (dev->AITaskSet)
	if (dev->AITaskSet->taskHndl) errChk(DAQmxStopTask(dev->AITaskSet->taskHndl));
   
	// AO task
	if (dev->AOTaskSet)
	if (dev->AOTaskSet->taskHndl) errChk(DAQmxStopTask(dev->AOTaskSet->taskHndl));
	
	// DI task
	if (dev->DITaskSet)
	if (dev->DITaskSet->taskHndl) errChk(DAQmxStopTask(dev->DITaskSet->taskHndl));
	
	// DO task
	if (dev->DOTaskSet)
	if (dev->DOTaskSet->taskHndl) errChk(DAQmxStopTask(dev->DOTaskSet->taskHndl));
	
	// CI task
	if (dev->CITaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems			= ListNumItems(dev->CITaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nItems; i++) {	
			chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if ((*chanSetPtrPtr)->taskHndl) DAQmxStopTask((*chanSetPtrPtr)->taskHndl);
		}
	}
	
	// CO task
	if (dev->COTaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems			= ListNumItems(dev->COTaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nItems; i++) {	
			chanSetPtrPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);	
			if ((*chanSetPtrPtr)->taskHndl) DAQmxStopTask((*chanSetPtrPtr)->taskHndl);
		}
	}
	
	return init_FCallReturn_type(0, "", "");
	
Error:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "StopDAQmxTasks", errMsg);
	free(errMsg);
	return fCallReturn;
}

static FCallReturn_type* ConfigDAQmxDevice (Dev_type* dev)
{
#define StartAllDAQmxTasks_Err_NULLDev		-1
	
	FCallReturn_type*	fCallReturn	= NULL;
	if (!dev) return init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLDev, "ConfigDAQmxDevice", "No reference to device");
	
	// clear previous DAQmx tasks if any
	ClearDAQmxTasks(dev);
	
	
	// Configure AI DAQmx Task
	if ((fCallReturn = ConfigDAQmxAITask(dev)))		goto Error;
	
	// Configure AO DAQmx Task
	if ((fCallReturn = ConfigDAQmxAOTask(dev)))		goto Error;	
	
	// Configure DI DAQmx Task
	if ((fCallReturn = ConfigDAQmxDITask(dev)))		goto Error;
	
	// Configure DO DAQmx Task
	if ((fCallReturn = ConfigDAQmxDOTask(dev)))		goto Error;
	
	// Configure CI DAQmx Task
	if ((fCallReturn = ConfigDAQmxCITask(dev)))		goto Error;
	
	// Configure CO DAQmx Task
	if ((fCallReturn = ConfigDAQmxCOTask(dev)))		goto Error;
	
	
	return init_FCallReturn_type(0, "", "");
	
Error:
	ClearDAQmxTasks(dev);
	return fCallReturn;
}

static FCallReturn_type* ConfigDAQmxAITask (Dev_type* dev) 
{
#define ConfigDAQmxAITask_Err_ChannelNotImplemented		-1
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtrPtr;
	FCallReturn_type*	fCallReturn		= NULL;
	
	if (!dev->AITaskSet) return NULL; // do nothing
	
	// check if there is at least one AI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nItems 			= ListNumItems(dev->AITaskSet->chanSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		if (!(*chanSetPtrPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the AI task if any channels require HW-timing
	if (!hwTimingFlag) return NULL;	// do nothing
	
	// create DAQmx AI task 
	char* taskName = GetTaskControlName(dev->taskController);
	AppendString(&taskName, " AI Task", -1);
	DAQmxErrChk (DAQmxCreateTask(taskName, &dev->AITaskSet->taskHndl));
	OKfree(taskName);
	
	// create AI channels
	nItems = ListNumItems(dev->AITaskSet->chanSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		switch((*chanSetPtrPtr)->chanType) {
			
			case Chan_AI_Voltage:
				
				DAQmxErrChk (DAQmxCreateAIVoltageChan(dev->AITaskSet->taskHndl, (*chanSetPtrPtr)->name, "", (*(ChanSet_AIAO_Voltage_type**)chanSetPtrPtr)->terminal, 
													  (*(ChanSet_AIAO_Voltage_type**)chanSetPtrPtr)->Vmin, (*(ChanSet_AIAO_Voltage_type**)chanSetPtrPtr)->Vmax, DAQmx_Val_Volts, NULL)); 
				
				break;
				
			case Chan_AI_Current:
				
				DAQmxErrChk (DAQmxCreateAICurrentChan(dev->AITaskSet->taskHndl, (*chanSetPtrPtr)->name, "", (*(ChanSet_AIAO_Current_type**)chanSetPtrPtr)->terminal, 
													  (*(ChanSet_AIAO_Current_type**)chanSetPtrPtr)->Imin, (*(ChanSet_AIAO_Current_type**)chanSetPtrPtr)->Imax, DAQmx_Val_Amps,
													  (*(ChanSet_AIAO_Current_type**)chanSetPtrPtr)->shuntResistorType, (*(ChanSet_AIAO_Current_type**)chanSetPtrPtr)->shuntResistorValue, NULL));
							  
				break;
				
			// add here more channel type cases
			
			default:
				fCallReturn = init_FCallReturn_type(ConfigDAQmxAITask_Err_ChannelNotImplemented, "ConfigDAQmxAITask", "Channel type not implemented");
				goto Error;
		}
	}
	
	// sample clock source
	// if no sample clock is given, then use OnboardClock by default
	if (!dev->AITaskSet->timing->sampClkSource) 
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_Src, "OnboardClock"));
	else
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_Src, dev->AITaskSet->timing->sampClkSource));
	
	// sample clock edge
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_ActiveEdge, dev->AITaskSet->timing->sampClkEdge));
	
	// sampling rate
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_Rate, *dev->AITaskSet->timing->refSampleRate));
	
	// set operation mode: finite, continuous
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampQuant_SampMode, dev->AITaskSet->timing->measMode));
	
	// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
	// if a reference clock is given, use it to synchronize the internal clock
	if (dev->AITaskSet->timing->refClkSource) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_RefClk_Src, dev->AITaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_RefClk_Rate, dev->AITaskSet->timing->refClkFreq));
	}
	
	// adjust manually the number of samples to acquire and  input buffer size to be even multiple of blocksizes and make sure that the input buffer is
	// at least equal or larger than the total number of samples per channel to be acquired in case of finite acquisition and at least the number of
	// default samples for continuous acquisition (which is adjusted automatically depending on the sampling rate)
	
	switch (dev->AITaskSet->timing->measMode) {
		int		quot;
		int		defaultBuffSize; 
		
		case MeasFinite:
			
			// set number of samples per channel
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, (uInt64) *dev->AITaskSet->timing->refNSamples));
			quot = *dev->AITaskSet->timing->refNSamples  / dev->AITaskSet->timing->blockSize; 
			if (quot % 2) quot++;
			if (!quot) quot = 1;
			DAQmxErrChk(DAQmxCfgInputBuffer (dev->AITaskSet->taskHndl, dev->AITaskSet->timing->blockSize * quot));
			break;
				
		case MeasCont:
			
			DAQmxGetBufferAttribute(dev->AITaskSet->taskHndl, DAQmx_Buf_Input_BufSize, &defaultBuffSize);
			quot = defaultBuffSize / dev->AITaskSet->timing->blockSize; 
			if (quot % 2) quot++;
			if (!quot) quot = 1;
			DAQmxErrChk(DAQmxCfgInputBuffer (dev->AITaskSet->taskHndl, dev->AITaskSet->timing->blockSize * quot));
			break;
	}
	
	//----------------------
	// Configure AI triggers
	//----------------------
	// configure start trigger
	if (dev->AITaskSet->startTrig)
		switch (dev->AITaskSet->startTrig->trigType) {
			case Trig_None:
				break;
			
			case Trig_AnalogEdge:
				DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->slope, dev->AITaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->wndTrigCond, 
							 dev->AITaskSet->startTrig->wndTop, dev->AITaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	// configure reference trigger
	if (dev->AITaskSet->referenceTrig)
		switch (dev->AITaskSet->referenceTrig->trigType) {
			case Trig_None:
				break;
			
			case Trig_AnalogEdge:
				DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->slope, dev->AITaskSet->referenceTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->wndTrigCond, 
							 dev->AITaskSet->referenceTrig->wndTop, dev->AITaskSet->referenceTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	//----------------------
	// Add AI Task callbacks
	//----------------------
	// register AI data available callback 
	DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent(dev->AITaskSet->taskHndl, DAQmx_Val_Acquired_Into_Buffer, dev->AITaskSet->timing->blockSize, 0, AIDAQmxTaskDataAvailable_CB, dev)); 
	// register AI task done event callback
	// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
	// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
	DAQmxErrChk (DAQmxRegisterDoneEvent(dev->AITaskSet->taskHndl, 0, AIDAQmxTaskDone_CB, dev));
	
		
	return NULL;
	
Error:
	return fCallReturn;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "ConfigDAQmxAITask", errMsg);
	free(errMsg);
	return fCallReturn;
}

static FCallReturn_type* ConfigDAQmxAOTask (Dev_type* dev)
{
#define ConfigDAQmxAOTask_Err_OutOfMemory				-1
#define ConfigDAQmxAOTask_Err_ChannelNotImplemented		-2
	
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtrPtr;
	FCallReturn_type*	fCallReturn		= NULL;
	
	if (!dev->AOTaskSet) return NULL;	// do nothing
	
	// clear and init writeAOData used for continuous streaming
	discard_WriteAOData_type(&dev->AOTaskSet->writeAOData);
	dev->AOTaskSet->writeAOData = init_WriteAOData_type(dev);
	if (!dev->AOTaskSet->writeAOData) goto MemError;
	
	// check if there is at least one AO task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nItems			= ListNumItems(dev->AOTaskSet->chanSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!(*chanSetPtrPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the AO task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create DAQmx AO task with hardware timing
	char* taskName = GetTaskControlName(dev->taskController);
	AppendString(&taskName, " AO Task", -1);
	DAQmxErrChk (DAQmxCreateTask(taskName, &dev->AOTaskSet->taskHndl));
	OKfree(taskName);
	
	// create AO channels
	nItems			= ListNumItems(dev->AOTaskSet->chanSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
	
		// include in the task only channels for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		switch((*chanSetPtrPtr)->chanType) {
			
			case Chan_AO_Voltage:
				
				// create channel
				DAQmxErrChk (DAQmxCreateAOVoltageChan(dev->AOTaskSet->taskHndl, (*chanSetPtrPtr)->name, "", (*(ChanSet_AIAO_Voltage_type**)chanSetPtrPtr)->Vmin, 
													  (*(ChanSet_AIAO_Voltage_type**)chanSetPtrPtr)->Vmax, DAQmx_Val_Volts, NULL)); 
				
				break;
				
			case Chan_AO_Current:
				
				DAQmxErrChk (DAQmxCreateAOCurrentChan(dev->AOTaskSet->taskHndl, (*chanSetPtrPtr)->name, "", (*(ChanSet_AIAO_Current_type**)chanSetPtrPtr)->Imin,
													  (*(ChanSet_AIAO_Current_type**)chanSetPtrPtr)->Imax, DAQmx_Val_Amps, NULL));
							  
				break;
			/*	
			case Chan_AO_Function:
			
				DAQmxErrChk (DAQmxCreateAOFuncGenChan(dev->AOTaskSet->taskHndl, (*chanSetPtrPtr)->name, "", ,
				
				break;
			*/
			
			default:
				fCallReturn = init_FCallReturn_type(ConfigDAQmxAOTask_Err_ChannelNotImplemented, "ConfigDAQmxAOTask", "Channel type not implemented");
				goto Error;
		}
	}
	
	// sample clock source
	// if no sample clock is given, then OnboardClock is used by default
	if (!dev->AOTaskSet->timing->sampClkSource) 
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampClk_Src, "OnboardClock"));
	else
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampClk_Src, dev->AOTaskSet->timing->sampClkSource));
	
	// sample clock edge
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampClk_ActiveEdge, dev->AOTaskSet->timing->sampClkEdge));
	
	// sampling rate
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampClk_Rate, *dev->AOTaskSet->timing->refSampleRate));
	
	// set operation mode: finite, continuous
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampQuant_SampMode, dev->AOTaskSet->timing->measMode));
	
	// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
	// set number of samples per channel for finite generation
	if (dev->AOTaskSet->timing->measMode == MeasFinite)
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, (uInt64) *dev->AOTaskSet->timing->refNSamples));
	
	// if a reference clock is given, use it to synchronize the internal clock
	if (dev->AOTaskSet->timing->refClkSource) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_RefClk_Src, dev->AOTaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_RefClk_Rate, dev->AOTaskSet->timing->refClkFreq));
	}
	
	// disable AO regeneration
	DAQmxSetWriteAttribute (dev->AOTaskSet->taskHndl, DAQmx_Write_RegenMode, DAQmx_Val_DoNotAllowRegen);
	
	// adjust output buffer size to be twice (even multiple) the blocksize
	DAQmxErrChk (DAQmxCfgOutputBuffer(dev->AOTaskSet->taskHndl, 2 * dev->AOTaskSet->timing->blockSize));
	
	//----------------------
	// Configure AO triggers
	//----------------------
	// configure start trigger
	if (dev->AOTaskSet->startTrig)
		switch (dev->AOTaskSet->startTrig->trigType) {
			case Trig_None:
				break;
			
			case Trig_AnalogEdge:
				DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->slope, dev->AOTaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->wndTrigCond, 
							 dev->AOTaskSet->startTrig->wndTop, dev->AOTaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	// configure reference trigger
	if (dev->AOTaskSet->referenceTrig)
		switch (dev->AOTaskSet->referenceTrig->trigType) {
			case Trig_None:
				break;
			
			case Trig_AnalogEdge:
				DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->slope, dev->AOTaskSet->referenceTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->wndTrigCond, 
							 dev->AOTaskSet->referenceTrig->wndTop, dev->AOTaskSet->referenceTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	//----------------------
	// Add AO Task callbacks
	//----------------------
	// register AO data request callback 
	DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent(dev->AOTaskSet->taskHndl, DAQmx_Val_Transferred_From_Buffer, dev->AOTaskSet->timing->blockSize, 0, AODAQmxTaskDataRequest_CB, dev)); 
	// register AO task done event callback
	// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
	// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
	DAQmxErrChk (DAQmxRegisterDoneEvent(dev->AOTaskSet->taskHndl, 0, AODAQmxTaskDone_CB, dev));
	
	return NULL;
Error:
	return fCallReturn;
	
MemError:
	return init_FCallReturn_type(ConfigDAQmxAOTask_Err_OutOfMemory, "ConfigDAQmxAOTask", "Out of memory");
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "ConfigDAQmxAOTask", errMsg);
	free(errMsg);
	return fCallReturn;
}

static FCallReturn_type* ConfigDAQmxDITask (Dev_type* dev)
{
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtrPtr;
	FCallReturn_type*	fCallReturn		= NULL;
	
	if (!dev->DITaskSet) return NULL;	// do nothing
	
	// check if there is at least one DI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nItems			= ListNumItems(dev->DITaskSet->chanSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
		if (!(*chanSetPtrPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the DI task if any channels require HW-timing
	if (!hwTimingFlag) return NULL;	// do nothing
	
	// create DAQmx DI task 
	char* taskName = GetTaskControlName(dev->taskController);
	AppendString(&taskName, " DI Task", -1);
	DAQmxErrChk (DAQmxCreateTask(taskName, &dev->DITaskSet->taskHndl));
	OKfree(taskName);
	
	// create DI channels
	nItems	= ListNumItems(dev->DITaskSet->chanSet); 
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		DAQmxErrChk (DAQmxCreateDIChan(dev->DITaskSet->taskHndl, (*chanSetPtrPtr)->name, "", DAQmx_Val_ChanForAllLines)); 
	}
				 
	// sample clock source
	// if no sample clock is given, then use OnboardClock by default
	if (!dev->DITaskSet->timing->sampClkSource) 
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampClk_Src, "OnboardClock"));
	else
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampClk_Src, dev->DITaskSet->timing->sampClkSource));
	
	// sample clock edge
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampClk_ActiveEdge, dev->DITaskSet->timing->sampClkEdge));
	
	// sampling rate
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampClk_Rate, *dev->DITaskSet->timing->refSampleRate));
	
	// set operation mode: finite, continuous
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampQuant_SampMode, dev->DITaskSet->timing->measMode));
	
	// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
	// if a reference clock is given, use it to synchronize the internal clock
	if (dev->DITaskSet->timing->refClkSource) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_RefClk_Src, dev->DITaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_RefClk_Rate, dev->DITaskSet->timing->refClkFreq));
	}
	
	// adjust manually number of samples and input buffer size to be even multiple of blocksizes and make sure that the input buffer is
	// at least equal or larger than the total number of samples per channel to be acquired in case of finite acquisition and at least the number of
	// default samples for continuous acquisition (which is adjusted automatically depending on the sampling rate)
	
	switch (dev->DITaskSet->timing->measMode) {
		int		quot;
		int		defaultBuffSize; 
		
		case MeasFinite:
			
			// set number of samples per channel read within one call of the read function, i.e. blocksize
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, (uInt64) *dev->DITaskSet->timing->refNSamples));
			quot = *dev->DITaskSet->timing->refNSamples  / dev->DITaskSet->timing->blockSize; 
			if (quot % 2) quot++;
			DAQmxErrChk(DAQmxCfgInputBuffer (dev->DITaskSet->taskHndl, dev->DITaskSet->timing->blockSize * quot));
			break;
				
		case MeasCont:
			
			DAQmxGetBufferAttribute(dev->DITaskSet->taskHndl, DAQmx_Buf_Input_BufSize, &defaultBuffSize);
			quot = defaultBuffSize / dev->DITaskSet->timing->blockSize; 
			if (quot % 2) quot++;
			DAQmxErrChk(DAQmxCfgInputBuffer (dev->DITaskSet->taskHndl, dev->DITaskSet->timing->blockSize * quot));
			break;
	}
	
	//----------------------
	// Configure DI triggers
	//----------------------
	// configure start trigger
	switch (dev->DITaskSet->startTrig->trigType) {
		case Trig_None:
			break;
			
		case Trig_AnalogEdge:
			DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->slope, dev->DITaskSet->startTrig->level));  
			break;
			
		case Trig_AnalogWindow:
			DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->wndTrigCond, 
						 dev->DITaskSet->startTrig->wndTop, dev->DITaskSet->startTrig->wndBttm));  
			break;
			
		case Trig_DigitalEdge:
			DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->slope));  
			break;
			
		case Trig_DigitalPattern:
			break;
	}
	
	// configure reference trigger
	switch (dev->DITaskSet->referenceTrig->trigType) {
		case Trig_None:
			break;
			
		case Trig_AnalogEdge:
			DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->slope, dev->DITaskSet->referenceTrig->level));  
			break;
			
		case Trig_AnalogWindow:
			DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->wndTrigCond, 
						 dev->DITaskSet->referenceTrig->wndTop, dev->DITaskSet->referenceTrig->wndBttm));  
			break;
			
		case Trig_DigitalEdge:
			DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->slope));  
			break;
			
		case Trig_DigitalPattern:
			break;
	}
	
	//----------------------
	// Add DI Task callbacks
	//----------------------
	// register DI data available callback 
	DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent(dev->DITaskSet->taskHndl, DAQmx_Val_Acquired_Into_Buffer, dev->DITaskSet->timing->blockSize, 0, DIDAQmxTaskDataAvailable_CB, dev)); 
	// register DI task done event callback (which is called also if the task encounters an error)
	// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
	// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
	DAQmxErrChk (DAQmxRegisterDoneEvent(dev->DITaskSet->taskHndl, 0, DIDAQmxTaskDone_CB, dev));
	
		
	return NULL;

DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "ConfigDAQmxDITask", errMsg);
	free(errMsg);
	return fCallReturn;	
}

static FCallReturn_type* ConfigDAQmxDOTask (Dev_type* dev)
{
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtrPtr;
	FCallReturn_type*	fCallReturn		= NULL;
	
	if (!dev->DOTaskSet) return NULL;	// do nothing
	
	// check if there is at least one DO task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nItems			= ListNumItems(dev->DOTaskSet->chanSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
		if (!(*chanSetPtrPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the DO task if any channels require HW-timing
	if (!hwTimingFlag) return NULL;	// do nothing
	
	// create DAQmx DO task 
	char* taskName = GetTaskControlName(dev->taskController);
	AppendString(&taskName, " DO Task", -1);
	DAQmxErrChk (DAQmxCreateTask(taskName, &dev->DOTaskSet->taskHndl));
	OKfree(taskName);
	
	// create DO channels
	nItems	= ListNumItems(dev->DOTaskSet->chanSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		DAQmxErrChk (DAQmxCreateDOChan(dev->DOTaskSet->taskHndl, (*chanSetPtrPtr)->name, "", DAQmx_Val_ChanForAllLines)); 
	}
				 
	// sample clock source
	// if no sample clock is given, then use OnboardClock by default
	if (!dev->DOTaskSet->timing->sampClkSource) 
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampClk_Src, "OnboardClock"));
	else
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampClk_Src, dev->DOTaskSet->timing->sampClkSource));
	
	// sample clock edge
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampClk_ActiveEdge, dev->DOTaskSet->timing->sampClkEdge));
	
	// sampling rate
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampClk_Rate, *dev->DOTaskSet->timing->refSampleRate));
	
	// set operation mode: finite, continuous
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampQuant_SampMode, dev->DOTaskSet->timing->measMode));
	
	// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
	// if a reference clock is given, use it to synchronize the internal clock
	if (dev->DOTaskSet->timing->refClkSource) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_RefClk_Src, dev->DOTaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_RefClk_Rate, dev->DOTaskSet->timing->refClkFreq));
	}
	
	// set number of samples per channel for finite acquisition
	if (dev->DOTaskSet->timing->measMode == MeasFinite)
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, (uInt64) *dev->DOTaskSet->timing->refNSamples));
	
	// disable DO regeneration
	DAQmxSetWriteAttribute (dev->DOTaskSet->taskHndl, DAQmx_Write_RegenMode, DAQmx_Val_DoNotAllowRegen);
	
	// adjust output buffer size to be even multiple of blocksize
	DAQmxErrChk (DAQmxCfgOutputBuffer(dev->DOTaskSet->taskHndl, 2 * dev->DOTaskSet->timing->blockSize));
	
	//----------------------
	// Configure DO triggers
	//----------------------
	// configure start trigger
	switch (dev->DOTaskSet->startTrig->trigType) {
		case Trig_None:
			break;
			
		case Trig_AnalogEdge:
			DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->slope, dev->DOTaskSet->startTrig->level));  
			break;
			
		case Trig_AnalogWindow:
			DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->wndTrigCond, 
						 dev->DOTaskSet->startTrig->wndTop, dev->DOTaskSet->startTrig->wndBttm));  
			break;
			
		case Trig_DigitalEdge:
			DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->slope));  
			break;
			
		case Trig_DigitalPattern:
			break;
	}
	
	// configure reference trigger
	switch (dev->DOTaskSet->referenceTrig->trigType) {
		case Trig_None:
			break;
			
		case Trig_AnalogEdge:
			DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->slope, dev->DOTaskSet->referenceTrig->level));  
			break;
			
		case Trig_AnalogWindow:
			DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->wndTrigCond, 
						 dev->DOTaskSet->referenceTrig->wndTop, dev->DOTaskSet->referenceTrig->wndBttm));  
			break;
			
		case Trig_DigitalEdge:
			DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->slope));  
			break;
			
		case Trig_DigitalPattern:
			break;
	}
	
	//----------------------
	// Add DO Task callbacks
	//----------------------
	// register DO data available callback 
	DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent(dev->DOTaskSet->taskHndl, DAQmx_Val_Acquired_Into_Buffer, dev->DOTaskSet->timing->blockSize, 0, DODAQmxTaskDataRequest_CB, dev)); 
	// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
	// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
	// register DO task done event callback (which is called also if the task encounters an error)
	DAQmxErrChk (DAQmxRegisterDoneEvent(dev->DOTaskSet->taskHndl, 0, DODAQmxTaskDone_CB, dev));
	
		
	return NULL;

DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "ConfigDAQmxDOTask", errMsg);
	free(errMsg);
	return fCallReturn;
}

static FCallReturn_type* ConfigDAQmxCITask (Dev_type* dev)
{
#define ConfigDAQmxCITask_Err_ChanNotImplemented		-1 
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtrPtr;
	FCallReturn_type*	fCallReturn		= NULL;
	
	if (!dev->CITaskSet) return NULL;	// do nothing
	
	// check if there is at least one CI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nItems			= ListNumItems(dev->CITaskSet->chanTaskSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		if (!(*chanSetPtrPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the CI task if any channels require HW-timing
	if (!hwTimingFlag) return NULL;	// do nothing
	
	// create CI channels
	nItems	= ListNumItems(dev->CITaskSet->chanTaskSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		// create DAQmx CI task for each counter
		char* taskName = GetTaskControlName(dev->taskController);
		AppendString(&taskName, " CI Task on ", -1);
		AppendString(&taskName, (*chanSetPtrPtr)->name, -1);
		DAQmxErrChk (DAQmxCreateTask(taskName, &(*chanSetPtrPtr)->taskHndl));
		OKfree(taskName);
				
		switch ((*chanSetPtrPtr)->chanType) {
				
			case Chan_CI_Frequency:
				
				ChanSet_CI_Frequency_type*	chCIFreqPtr = *(ChanSet_CI_Frequency_type**)chanSetPtrPtr;
				
				DAQmxErrChk (DAQmxCreateCIFreqChan((*chanSetPtrPtr)->taskHndl, (*chanSetPtrPtr)->name, "", chCIFreqPtr->freqMin, chCIFreqPtr->freqMax, DAQmx_Val_Hz, 
												   chCIFreqPtr->edgeType, chCIFreqPtr->measMethod, chCIFreqPtr->measTime, chCIFreqPtr->divisor, NULL));
				
				// use another terminal for the counter if other than default
				if (chCIFreqPtr->freqInputTerminal)
				DAQmxErrChk (DAQmxSetChanAttribute((*chanSetPtrPtr)->taskHndl, (*chanSetPtrPtr)->name, DAQmx_CI_Freq_Term, chCIFreqPtr->freqInputTerminal));
				
				// sample clock source
				// if no sample clock is given, then use OnboardClock by default
				if (!chCIFreqPtr->timing->sampClkSource) 
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_SampClk_Src, "OnboardClock"));
				else
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_SampClk_Src, chCIFreqPtr->timing->sampClkSource));
	
				// sample clock edge
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_SampClk_ActiveEdge, chCIFreqPtr->timing->sampClkEdge));
	
				// sampling rate
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_SampClk_Rate, *chCIFreqPtr->timing->refSampleRate));
	
				// set operation mode: finite, continuous
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_SampQuant_SampMode, chCIFreqPtr->timing->measMode));
	
				// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCIFreqPtr->timing->refClkSource) {
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Src, chCIFreqPtr->timing->refClkSource));
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->timing->refClkFreq));
				}
	
				// set number of samples per channel for finite acquisition
				if (chCIFreqPtr->timing->measMode == MeasFinite)
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_SampQuant_SampPerChan, (uInt64) *chCIFreqPtr->timing->refNSamples));
				
				/*
				DAQmxErrChk (DAQmxSetTrigAttribute(gTaskHandle,DAQmx_ArmStartTrig_Type,DAQmx_Val_DigEdge));
				DAQmxErrChk (DAQmxSetTrigAttribute(gTaskHandle,DAQmx_DigEdge_ArmStartTrig_Src,sampleClkSrc));
				DAQmxErrChk (DAQmxSetTrigAttribute(gTaskHandle,DAQmx_DigEdge_ArmStartTrig_Edge,DAQmx_Val_Rising));
				*/
				
				
				
				
				break;
				
			default:
				fCallReturn = init_FCallReturn_type(ConfigDAQmxCITask_Err_ChanNotImplemented, "ConfigDAQmxCITask", "Channel type not implemented");
				goto Error;
		}
	}
				 
	
	return NULL;
Error:
	return fCallReturn;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "ConfigDAQmxCITask", errMsg);
	free(errMsg);
	return fCallReturn;
}


void CheckName(char* name)
{
	int i;
	for (i=0;i<strlen(name);i++){
		if (name[i]=='/') name[i]='_';
	}
}

static FCallReturn_type* ConfigDAQmxCOTask (Dev_type* dev)
{
	#define ConfigDAQmxCOTask_Err_ChanNotImplemented		-1 
	int 				error 			= 0;
	FCallReturn_type*   fCallReturn		= NULL;
	ChanSet_type** 		chanSetPtrPtr;       
	
		if (!dev->COTaskSet) return NULL;	// do nothing
	
	// check if there is at least one CI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nItems			= ListNumItems(dev->COTaskSet->chanTaskSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		if (!(*chanSetPtrPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the CI task if any channels require HW-timing
	if (!hwTimingFlag) return NULL;	// do nothing
	
	// create CO channels
	nItems	= ListNumItems(dev->COTaskSet->chanTaskSet);
	for (size_t i = 1; i <= nItems; i++) {	
		chanSetPtrPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		ChanSet_CO_type*	chCOPtr = *(ChanSet_CO_type**)chanSetPtrPtr;  
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		// create DAQmx CO task for each counter
		char* taskName=GetTaskControlName(dev->taskController);
		
		
		AppendString(&taskName, " CO Task on ", -1);
		AppendString(&taskName, (*chanSetPtrPtr)->name, -1);
		CheckName(taskName);   
		DAQmxErrChk (DAQmxCreateTask(taskName, &(*chanSetPtrPtr)->taskHndl));
		OKfree(taskName);
				
		switch ((*chanSetPtrPtr)->chanType) {
				
			case Chan_CO_Pulse_Frequency:
			
				DAQmxErrChk (DAQmxCreateCOPulseChanFreq((*chanSetPtrPtr)->taskHndl, (*chanSetPtrPtr)->name, "", chCOPtr->units, chCOPtr->idlestate, chCOPtr->initialdel, 
												   chCOPtr->frequency, (chCOPtr->dutycycle/100)));
	
				
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCOPtr->baseClass.referenceTrig->trigType==DAQmx_Val_DigEdge) {
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Src, chCOPtr->baseClass.referenceTrig->trigSource));
			//		DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->timing->refClkFreq));
				}
					// if no reference clock is given, then use OnboardClock by default
			
	
			
				// trigger 	
				if (chCOPtr->baseClass.startTrig->trigType==DAQmx_Val_DigEdge){
				//	DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_StartTrig_Type,DAQmx_Val_DigEdge));
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_DigEdge_StartTrig_Src,chCOFreqPtr->baseClass.startTrig->trigSource));
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_DigEdge_StartTrig_Edge,chCOFreqPtr->baseClass.startTrig->slope));
					DAQmxErrChk (DAQmxCfgDigEdgeStartTrig((*chanSetPtrPtr)->taskHndl,chCOPtr->baseClass.startTrig->trigSource,chCOPtr->baseClass.startTrig->slope));  
				}
			//	else 
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_ArmStartTrig_Type,DAQmx_Val_None));  
			
				DAQmxErrChk (DAQmxCfgImplicitTiming((*chanSetPtrPtr)->taskHndl,chCOPtr->timing->measMode, chCOPtr->timing->nSamples));
				
				break;
				
			case Chan_CO_Pulse_Time:
				
				DAQmxErrChk (DAQmxCreateCOPulseChanTime((*chanSetPtrPtr)->taskHndl, (*chanSetPtrPtr)->name, "", chCOPtr->units, chCOPtr->idlestate, chCOPtr->initialdel, 
												   chCOPtr->lowtime, chCOPtr->hightime));
	
				
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCOPtr->baseClass.referenceTrig->trigType==DAQmx_Val_DigEdge) {
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Src, chCOPtr->baseClass.referenceTrig->trigSource));
			//		DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->timing->refClkFreq));
				}
					// if no reference clock is given, then use OnboardClock by default
			
	
			
				// trigger 	
				if (chCOPtr->baseClass.startTrig->trigType==DAQmx_Val_DigEdge){
				//	DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_StartTrig_Type,DAQmx_Val_DigEdge));
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_DigEdge_StartTrig_Src,chCOFreqPtr->baseClass.startTrig->trigSource));
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_DigEdge_StartTrig_Edge,chCOFreqPtr->baseClass.startTrig->slope));
					DAQmxErrChk (DAQmxCfgDigEdgeStartTrig((*chanSetPtrPtr)->taskHndl,chCOPtr->baseClass.startTrig->trigSource,chCOPtr->baseClass.startTrig->slope));  
				}
			//	else 
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_ArmStartTrig_Type,DAQmx_Val_None));  
			
				DAQmxErrChk (DAQmxCfgImplicitTiming((*chanSetPtrPtr)->taskHndl,chCOPtr->timing->measMode, chCOPtr->timing->nSamples));
				
				break;
				
			case Chan_CO_Pulse_Ticks:
			
				DAQmxErrChk (DAQmxCreateCOPulseChanTicks((*chanSetPtrPtr)->taskHndl, (*chanSetPtrPtr)->name, "",chCOPtr->baseClass.referenceTrig->trigSource , chCOPtr->idlestate, chCOPtr->initialdel, 
												   chCOPtr->lowticks, chCOPtr->highticks));
	
				
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCOPtr->baseClass.referenceTrig->trigType==DAQmx_Val_DigEdge) {
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Src, chCOPtr->baseClass.referenceTrig->trigSource));
			//		DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->timing->refClkFreq));
				}
					// if no reference clock is given, then use OnboardClock by default
			
	
			
				// trigger 	
				if (chCOPtr->baseClass.startTrig->trigType==DAQmx_Val_DigEdge){
				//	DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_StartTrig_Type,DAQmx_Val_DigEdge));
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_DigEdge_StartTrig_Src,chCOFreqPtr->baseClass.startTrig->trigSource));
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_DigEdge_StartTrig_Edge,chCOFreqPtr->baseClass.startTrig->slope));
					DAQmxErrChk (DAQmxCfgDigEdgeStartTrig((*chanSetPtrPtr)->taskHndl,chCOPtr->baseClass.startTrig->trigSource,chCOPtr->baseClass.startTrig->slope));  
				}
			//	else 
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtrPtr)->taskHndl,DAQmx_ArmStartTrig_Type,DAQmx_Val_None));  
			
				DAQmxErrChk (DAQmxCfgImplicitTiming((*chanSetPtrPtr)->taskHndl,chCOPtr->timing->measMode, chCOPtr->timing->nSamples));
				
			break;
				
			default:
				fCallReturn = init_FCallReturn_type(ConfigDAQmxCOTask_Err_ChanNotImplemented, "ConfigDAQmxCOTask", "Channel type not implemented");
				goto Error;
		}
		
		// register CO task done event callback
		// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
		// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
		DAQmxErrChk (DAQmxRegisterDoneEvent((*chanSetPtrPtr)->taskHndl, 0, CODAQmxTaskDone_CB, dev));
	}
	
	
	
	return NULL;
Error:
	return fCallReturn;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "ConfigDAQmxCOTask", errMsg);
	free(errMsg);
	return fCallReturn;
}

///HIFN Writes data continuously to a DAQmx AO.
///HIRET NULL if there is no error and FCallReturn_type* data in case of error. 
FCallReturn_type* WriteAODAQmx (Dev_type* dev) 
{

	DataPacket_type* 		dataPacket;
	DLDataTypes				dataPacketType;
	void*					dataPacketData;
	double*					waveformData			= NULL;
	WriteAOData_type*    	data            		= dev->AOTaskSet->writeAOData;
	size_t          		queue_items;
	size_t          		ncopies;                			// number of datapacket copies to fill at least a writeblock size
	size_t         		 	numwrote;
	double					nRepeats				= 1;
	int             		error           		= 0;
	float64*        		tmpbuff;
	FCallReturn_type*		fCallReturn				= NULL;
	BOOL					writeBuffersFilledFlag	= TRUE;  	// assume that each output channel has at least data->writeblock elements 
	size_t					nSamples;
	
	// cycle over channels
	for (int i = 0; i < data->numchan; i++) {
		CmtTSQHandle	tsqID = GetSinkVChanTSQHndl(data->sinkVChans[i]);
		while (data->databuff_size[i] < data->writeblock) {
			
			// if datain[i] is empty, get data packet from queue
			if (!data->datain_size[i]) {
				
				// check if there are any data packets in the queue
				CmtGetTSQAttribute(tsqID, ATTR_TSQ_ITEMS_IN_QUEUE, &queue_items);
				
				// if there are no more data packets, then skip this channel
				if (!queue_items) {
					writeBuffersFilledFlag = FALSE;				// there is not enough data for this channel 
					break;
				}
				
				// get data packet from queue
				CmtReadTSQData (tsqID, &dataPacket,1,0,0);
				
				// copy data packet to datain
				dataPacketData = GetDataPacketDataPtr(dataPacket, &dataPacketType);
				switch (dataPacketType) {
					case DL_Waveform_Double:
						waveformData = GetWaveformDataPtr(dataPacketData, &data->datain_size[i]);
						nRepeats = 1;
						break;
						
					case DL_RepeatedWaveform_Double:
						waveformData = GetRepeatedWaveformDataPtr(dataPacketData, &data->datain_size[i]);
						nRepeats = GetRepeatedWaveformRepeats(dataPacketData);
						break;
				}
				data->datain[i] = malloc (data->datain_size[i] * sizeof(float64));
				memcpy(data->datain[i], waveformData, data->datain_size[i] * sizeof(float64));
				
				// copy repeats
				if (nRepeats) {
					data->datain_repeat[i]    = (size_t) nRepeats;
					data->datain_remainder[i] = (size_t) ((nRepeats - (double) data->datain_repeat[i]) * (double) data->datain_size[i]);
					data->datain_loop[i]      = 0;
				} else data->datain_loop[i]   = 1;
				
				// data packet not needed anymore, release it (data is deleted if there are no other sinks that need it)
				ReleaseDataPacket(&dataPacket); 
			}
			
			// get number of datain copies needed to fill the rest of databuff
			ncopies = (data->writeblock - data->databuff_size[i]) /data->datain_size[i] + 1;
			
			if ((ncopies < data->datain_repeat[i]) || data->datain_loop[i]) {
				// if there are sufficient repeats in datain which may have finite or infinte repeats
				// allocate memory for buffer
				data->databuff[i] = realloc (data->databuff[i], (data->databuff_size[i] + ncopies * data->datain_size[i]) * sizeof(float64));
				
				// insert one or more data copies in the buffer
				for (int j = 0; j < ncopies; j++)
					memcpy(data->databuff[i] + data->databuff_size[i] + j * data->datain_size[i], data->datain[i], data->datain_size[i] * sizeof (float64));
				
				// update number of data elements in the buffer
				data->databuff_size[i] += ncopies * data->datain_size[i];
				
				// if repeats is finite,  update number of repeats left in datain
				if (!data->datain_loop[i]) 
					data->datain_repeat[i] -= ncopies;
				else {
					// if repeats is infinite, check if there is another data packet waiting in the queue
					CmtGetTSQAttribute(tsqID, ATTR_TSQ_ITEMS_IN_QUEUE, &queue_items);
					
					// if there is another data packet in the queue, then switch to the new packet by freeing datain[i]
					if (queue_items) {
						OKfree(data->datain[i]);
						data->datain_size[i] = 0;
					}
				
				}
				
				break; // this branch satisfies data->databuff_size[i] > data->writeblock and the while loop can exit
				
			} else 
				if (data->datain_repeat[i]) {
					// if the repeats in datain are not sufficient or they are just the right amount
					// allocate memory for buffer
					data->databuff[i] = realloc (data->databuff[i], (data->databuff_size[i] + data->datain_repeat[i] * data->datain_size[i]) * sizeof(float64));
					
					// insert the datain[i] in the buffer repeat times
					for (int j = 0; j < data->datain_repeat[i]; j++)
						memcpy(data->databuff[i] + data->databuff_size[i] + j * data->datain_size[i], data->datain[i], data->datain_size[i] * sizeof (float64));
			
					// update number of data elements in the buffer
					data->databuff_size[i] += data->datain_repeat[i] * data->datain_size[i];
					
					// no more repeats left
					data->datain_repeat[i] = 0;
				
				} else {
					
					// copy remainding elements if there are any
					if (data->datain_remainder[i]){
						data->databuff[i] = realloc (data->databuff[i], (data->databuff_size[i] + data->datain_remainder[i]) * sizeof(float64));
						memcpy(data->databuff[i] + data->databuff_size[i], data->datain[i], data->datain_remainder[i] * sizeof(float64));
						data->databuff_size[i] += data->datain_remainder[i];
						data->datain_remainder[i] = 0;
					}
					
					// datain[i] not needed anymore
					OKfree(data->datain[i]);
					data->datain_size[i] = 0;
				
				}
			
		}
	}
	
	
	if (writeBuffersFilledFlag) nSamples = data->writeblock;
	else {
		// determine the minimum number of samples available for all channels that can be written
		nSamples = data->databuff_size[0];
		for (size_t i = 1; i < data->numchan; i++)
			if (data->databuff_size[i] < nSamples) nSamples = data->databuff_size[i];
	}
	
	// do nothing if there are no available samples
	if (!nSamples) return NULL;
	
	// build dataout from buffers and at the same time, update elements left in the buffers
	for (size_t i = 0; i < data->numchan; i++) {
		// build dataout with one writeblock from databuff
		memcpy(data->dataout + i * nSamples, data->databuff[i], nSamples * sizeof(float64));
		// keep remaining data
		tmpbuff = malloc ((data->databuff_size[i] - nSamples) * sizeof(float64));
		memcpy(tmpbuff, data->databuff[i] + nSamples, (data->databuff_size[i] - nSamples) * sizeof(float64)); 
		// clean buffer and restore remaining data
		OKfree(data->databuff[i]);
		data->databuff[i] = tmpbuff;
		// update number of elements in databuff[i]
		data->databuff_size[i] -= nSamples;
	}
	
	DAQmxErrChk(DAQmxWriteAnalogF64(dev->AOTaskSet->taskHndl, nSamples, 0, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, data->dataout, &numwrote, NULL));
	 
	return NULL; 
	
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "WriteAODAQmx", errMsg);
	free(errMsg);
	
Error:
	
	ClearDAQmxTasks(dev);
	return fCallReturn;
}	 

static BOOL	DAQmxTasksDone(Dev_type* dev)
{
	BOOL taskDoneFlag;
	
	// AI task
	if (dev->AITaskSet)
	if (dev->AITaskSet->taskHndl) {
		DAQmxGetTaskAttribute(dev->AITaskSet->taskHndl, DAQmx_Task_Complete, &taskDoneFlag);
		if (!taskDoneFlag) return FALSE;
	}
	
	// AO task
	if (dev->AOTaskSet)
	if (dev->AOTaskSet->taskHndl) {
		DAQmxGetTaskAttribute(dev->AOTaskSet->taskHndl, DAQmx_Task_Complete, &taskDoneFlag); 
		if (!taskDoneFlag) return FALSE; 
	}
	
	// DI task
	if (dev->DITaskSet)
	if (dev->DITaskSet->taskHndl) {
		DAQmxGetTaskAttribute(dev->DITaskSet->taskHndl, DAQmx_Task_Complete, &taskDoneFlag);   
		if (!taskDoneFlag) return FALSE;
	}
	
	// DO task
	if (dev->DOTaskSet)
	if (dev->DOTaskSet->taskHndl) {
		DAQmxGetTaskAttribute(dev->DOTaskSet->taskHndl, DAQmx_Task_Complete, &taskDoneFlag);  
		if (!taskDoneFlag) return FALSE;
	}
	
	// CI task
	if (dev->CITaskSet) {
	   ChanSet_type** 	chanSetPtrPtr;
	   size_t			nItems			= ListNumItems(dev->CITaskSet->chanTaskSet);
	   for (size_t i = 1; i <= nItems; i++) {
		   chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		   if ((*chanSetPtrPtr)->taskHndl) {
			   DAQmxGetTaskAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_Task_Complete, &taskDoneFlag); 
			   if (!taskDoneFlag) return FALSE;
		   }
	   }
	}
	
	// CO task
	if (dev->COTaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		size_t			nItems			= ListNumItems(dev->COTaskSet->chanTaskSet);
	   for (size_t i = 1; i <= nItems; i++) {
		   chanSetPtrPtr= ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		   if ((*chanSetPtrPtr)->taskHndl) {
				DAQmxGetTaskAttribute((*chanSetPtrPtr)->taskHndl, DAQmx_Task_Complete, &taskDoneFlag); 
				if (!taskDoneFlag) return FALSE;
		   }
	   }
	}
	
	return TRUE;
}

static FCallReturn_type* StartAllDAQmxTasks (Dev_type* dev)
{
#define	StartAllDAQmxTasks_Err_OutOfMem		-1
#define StartAllDAQmxTasks_Err_NULLDev		-2
#define StartAllDAQmxTasks_Err_NULLTask		-3
	
	int					error;
	FCallReturn_type*   fCallReturn			= NULL;
	ListType 			NonTriggeredTasks	= 0;
	ListType 			TriggeredTasks		= 0;
	BOOL				triggeredFlag;
	size_t				nItems;
	
	if (!dev) {
		fCallReturn = init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLDev, "StartAllDAQmxTasks", "No reference to device");
		goto Error;
	}
	
	if (! (NonTriggeredTasks = ListCreate(sizeof(TaskHandle))) ) goto MemError;  // DAQmx tasks without any HW-trigger defined
	if (! (TriggeredTasks = ListCreate(sizeof(TaskHandle))) ) goto MemError;  // DAQmx tasks with HW-triggering
	
	//--------------------------------------------
	// AI task
	//--------------------------------------------
	if (!dev->AITaskSet) goto SkipAITask;
	// check task handle
	if (!dev->AITaskSet->taskHndl) {
		fCallReturn = init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLTask, "StartAllDAQmxTasks", "AI Task handle is missing");
		goto Error;
	}
	// start trig
	triggeredFlag = FALSE;
	if (dev->AITaskSet->startTrig)
		if (dev->AITaskSet->startTrig->trigType != Trig_None) 
			triggeredFlag = TRUE;
	// ref trig
	if (dev->AITaskSet->referenceTrig)
		if (dev->AITaskSet->referenceTrig->trigType != Trig_None)
			triggeredFlag = TRUE;
	// add to one of the lists
	if (triggeredFlag)
		ListInsertItem(TriggeredTasks, &dev->AITaskSet->taskHndl, END_OF_LIST);
	else
		ListInsertItem(NonTriggeredTasks, &dev->AITaskSet->taskHndl, END_OF_LIST);
	
SkipAITask:
	//--------------------------------------------
	// AO task
	//--------------------------------------------
	if (!dev->AOTaskSet) goto SkipAOTask; 
	// check task handle
	if (!dev->AOTaskSet->taskHndl) {
		fCallReturn = init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLTask, "StartAllDAQmxTasks", "AO Task handle is missing");
		goto Error;
	}
	// start trig
	triggeredFlag = FALSE;
	if (dev->AOTaskSet->startTrig)
		if (dev->AOTaskSet->startTrig->trigType != Trig_None) 
			triggeredFlag = TRUE;
	// ref trig
	if (dev->AOTaskSet->referenceTrig)
		if (dev->AOTaskSet->referenceTrig->trigType != Trig_None)
			triggeredFlag = TRUE;
	// add to one of the lists
	if (triggeredFlag)
		ListInsertItem(TriggeredTasks, &dev->AOTaskSet->taskHndl, END_OF_LIST);
	else
		ListInsertItem(NonTriggeredTasks, &dev->AOTaskSet->taskHndl, END_OF_LIST);
	
SkipAOTask:
	//--------------------------------------------
	// DI task
	//--------------------------------------------
	if (!dev->DITaskSet) goto SkipDITask; 
	// check task handle
	if (!dev->DITaskSet->taskHndl) {
		fCallReturn = init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLTask, "StartAllDAQmxTasks", "DI Task handle is missing");
		goto Error;
	}
	// start trig
	triggeredFlag = FALSE;
	if (dev->DITaskSet->startTrig)
		if (dev->DITaskSet->startTrig->trigType != Trig_None) 
			triggeredFlag = TRUE;
	// ref trig
	if (dev->DITaskSet->referenceTrig)
		if (dev->DITaskSet->referenceTrig->trigType != Trig_None)
			triggeredFlag = TRUE;
	// add to one of the lists
	if (triggeredFlag)
		ListInsertItem(TriggeredTasks, &dev->DITaskSet->taskHndl, END_OF_LIST);
	else
		ListInsertItem(NonTriggeredTasks, &dev->DITaskSet->taskHndl, END_OF_LIST);
	
SkipDITask:
	//--------------------------------------------
	// DO task
	//--------------------------------------------
	if (!dev->DOTaskSet) goto SkipDOTask;
	// check task handle
	if (!dev->DOTaskSet->taskHndl) {
		fCallReturn = init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLTask, "StartAllDAQmxTasks", "DO Task handle is missing");
		goto Error;
	}
	// start trig
	triggeredFlag = FALSE;
	if (dev->DOTaskSet->startTrig)
		if (dev->DOTaskSet->startTrig->trigType != Trig_None) 
			triggeredFlag = TRUE;
	// ref trig
	if (dev->DOTaskSet->referenceTrig)
		if (dev->DOTaskSet->referenceTrig->trigType != Trig_None)
			triggeredFlag = TRUE;
	// add to one of the lists
	if (triggeredFlag)
		ListInsertItem(TriggeredTasks, &dev->DOTaskSet->taskHndl, END_OF_LIST);
	else
		ListInsertItem(NonTriggeredTasks, &dev->DOTaskSet->taskHndl, END_OF_LIST);
	
SkipDOTask:
	//--------------------------------------------
	// CI tasks
	//--------------------------------------------
	if (dev->CITaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		nItems = ListNumItems(dev->CITaskSet->chanTaskSet);
		for (size_t i = 1; i <= nItems; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if ((*chanSetPtrPtr)->taskHndl) {
				// start trig
				triggeredFlag = FALSE;
				if ((*chanSetPtrPtr)->startTrig)
					if ((*chanSetPtrPtr)->startTrig->trigType != Trig_None) 
						triggeredFlag = TRUE;
				// ref trig
				if ((*chanSetPtrPtr)->referenceTrig)
					if ((*chanSetPtrPtr)->referenceTrig->trigType != Trig_None)
						triggeredFlag = TRUE;
				// add to one of the lists
				if (triggeredFlag)
					ListInsertItem(TriggeredTasks, &(*chanSetPtrPtr)->taskHndl, END_OF_LIST);
				else
					ListInsertItem(NonTriggeredTasks, &(*chanSetPtrPtr)->taskHndl, END_OF_LIST);
				
			} else {
				fCallReturn = init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLTask, "StartAllDAQmxTasks", "CI Task handle is missing");
				goto Error;
			}
		}
	}
	
	//--------------------------------------------
	// CO tasks
	//--------------------------------------------
	if (dev->COTaskSet) {
		ChanSet_type** 	chanSetPtrPtr;
		nItems = ListNumItems(dev->COTaskSet->chanTaskSet);
		for (size_t i = 1; i <= nItems; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
			if ((*chanSetPtrPtr)->taskHndl) {
				// start trig
				triggeredFlag = FALSE;
				if ((*chanSetPtrPtr)->startTrig)
					if ((*chanSetPtrPtr)->startTrig->trigType != Trig_None) 
						triggeredFlag = TRUE;
				// ref trig
				if ((*chanSetPtrPtr)->referenceTrig)
					if ((*chanSetPtrPtr)->referenceTrig->trigType != Trig_None)
						triggeredFlag = TRUE;
				// add to one of the lists
				if (triggeredFlag)
					ListInsertItem(TriggeredTasks, &(*chanSetPtrPtr)->taskHndl, END_OF_LIST);
				else
					ListInsertItem(NonTriggeredTasks, &(*chanSetPtrPtr)->taskHndl, END_OF_LIST);
				
			} else {
				fCallReturn = init_FCallReturn_type(StartAllDAQmxTasks_Err_NULLTask, "StartAllDAQmxTasks", "CO Task handle is missing");
				goto Error;
			}
		}
	}
	
	//----------------------
	// Start Triggered Tasks
	//----------------------
	TaskHandle* 	taskHndlPtr;
	
	nItems = ListNumItems(TriggeredTasks);
	for (size_t i = 1; i <= nItems; i++) {
		taskHndlPtr = ListGetPtrToItem(TriggeredTasks, i);
		DAQmxErrChk(DAQmxStartTask(*taskHndlPtr));
	}
	
	//--------------------------
	// Start Non-Triggered Tasks
	//--------------------------
	nItems = ListNumItems(NonTriggeredTasks);
	for (size_t i = 1; i <= nItems; i++) {
		taskHndlPtr = ListGetPtrToItem(NonTriggeredTasks, i);
		DAQmxErrChk(DAQmxStartTask(*taskHndlPtr));
	}
	
	// cleanup
	ListDispose(NonTriggeredTasks);
	ListDispose(TriggeredTasks);
	
	return NULL;
	
MemError:
	if (NonTriggeredTasks) ListDispose(NonTriggeredTasks);
	if (TriggeredTasks) ListDispose(TriggeredTasks);
	return init_FCallReturn_type(StartAllDAQmxTasks_Err_OutOfMem, "StartAllDAQmxTasks","Out of memory");
	
DAQmxError:  
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "StartAllDAQmxTasks", errMsg);
	free(errMsg);
	// stop all triggered tasks
	nItems = ListNumItems(TriggeredTasks);
	for (size_t i = 1; i <= nItems; i++) {
		taskHndlPtr = ListGetPtrToItem(TriggeredTasks, i);
		DAQmxErrChk(DAQmxStopTask(*taskHndlPtr));
	}
	// stop all non-triggered tasks
	nItems = ListNumItems(NonTriggeredTasks);
	for (size_t i = 1; i <= nItems; i++) {
		taskHndlPtr = ListGetPtrToItem(NonTriggeredTasks, i);
		DAQmxErrChk(DAQmxStopTask(*taskHndlPtr));
	}
	
	// fall through here
Error:
	if (NonTriggeredTasks) ListDispose(NonTriggeredTasks);
	if (TriggeredTasks) ListDispose(TriggeredTasks);
	return fCallReturn;
	
}

static BOOL	OutputBuffersFilled	(Dev_type* dev)
{
	BOOL				AOFilledFlag;
	BOOL				DOFilledFlag;
	unsigned int		nSamples;
	
	//--------------------------------------------
	// AO task
	//--------------------------------------------
	if (dev->AOTaskSet) {
		DAQmxGetWriteAttribute(dev->AOTaskSet->taskHndl, DAQmx_Write_SpaceAvail, &nSamples); 
		if (!nSamples) AOFilledFlag = TRUE;													 // no more space in the buffer, buffer filled
		else 
			if (dev->AOTaskSet->timing->measMode == MeasCont) AOFilledFlag = FALSE;			 // if continuous mode and there is space in the buffer, then buffer is not filled
				else
					if (*dev->AOTaskSet->timing->refNSamples == 2*dev->AOTaskSet->timing->blockSize - nSamples)  AOFilledFlag = TRUE; // otherwise if finite and the number of samples is exactly as requested, then buffer is considered filled.
						else AOFilledFlag = FALSE;
	} else
		AOFilledFlag = TRUE;   // buffer considered filled if there is no AO task.
	
	//--------------------------------------------
	// DO task
	//--------------------------------------------
	if (dev->DOTaskSet) {
		DAQmxGetWriteAttribute(dev->DOTaskSet->taskHndl, DAQmx_Write_SpaceAvail, &nSamples); 
		if (!nSamples) DOFilledFlag = TRUE;
		else DOFilledFlag = FALSE;
	} else
		DOFilledFlag = TRUE;
	
	return AOFilledFlag && DOFilledFlag;
}

static FCallReturn_type* FillOutputBuffer(Dev_type* dev) 
{
	FCallReturn_type*	fCallReturn		= NULL;
	unsigned int		nSamples;
	
	if (!dev->AOTaskSet || !dev->AOTaskSet->taskHndl) goto SkipAOBuffers; // no analog output buffers  
	
	// reserve AO task
	DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Reserve);
	
	DAQmxGetWriteAttribute(dev->AOTaskSet->taskHndl, DAQmx_Write_SpaceAvail, &nSamples);
	// try to fill buffer completely, i.e. the buffer has twice the blocksize number of samples
	if(nSamples) // if there is space in the buffer
		if (WriteAODAQmx(dev)) goto Error;
	// check again if buffer is filled
	DAQmxGetWriteAttribute(dev->AOTaskSet->taskHndl, DAQmx_Write_SpaceAvail, &nSamples);					
	if(nSamples) // if there is space in the buffer
		if (WriteAODAQmx(dev)) goto Error;
					
SkipAOBuffers:
							
	return NULL; // no error
	
Error:
	
	return fCallReturn;
}

int32 CVICALLBACK AIDAQmxTaskDataAvailable_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
#define AIDAQmxTaskDataAvailable_CB_Err_OutOfMemory		-1 
#define AIDAQmxTaskDone_CB_Err_SendDataPacket			-2  
	
	Dev_type*			dev 			= callbackData;
	float64*    		readBuffer		= NULL;				// temporary buffer to place data into
	uInt32				nAI;
	int					nRead;
	int					error			= 0;

	
	
	// allocate memory to read samples
	DAQmxGetTaskAttribute(taskHandle, DAQmx_Task_NumChans, &nAI);
	readBuffer = malloc(nSamples * nAI * sizeof(float64));
	if (!readBuffer) goto MemError;
	
	// read samples from the AI buffer
	DAQmxErrChk(DAQmxReadAnalogF64(taskHandle, nSamples, dev->AITaskSet->timeout, DAQmx_Val_GroupByChannel , readBuffer, nSamples * nAI, &nRead, NULL));
	
	// forward data to Source VChan
	ChanSet_type**		chanSetPtrPtr;
	size_t				nItems			= ListNumItems(dev->AITaskSet->chanSet);
	size_t				chIdx			= 0;
	double*				waveformData;
	Waveform_type*		waveform; 
	DataPacket_type*	dataPacket;
	FCallReturn_type*	fCallReturn;
	for (size_t i = 1; i <= nItems; i++) {
		chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		// include only channels for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		// create waveform
		waveformData = malloc(nSamples * sizeof(double));
		if (!waveformData) goto MemError;
		memcpy(waveformData, readBuffer + chIdx * nSamples, nSamples * sizeof(double));
		waveform = init_Waveform_type(Waveform_Double, *dev->AITaskSet->timing->refSampleRate, nSamples, waveformData);
		dataPacket = init_DataPacket_type(DL_Waveform_Double, waveform, (DiscardPacketDataFptr_type) discard_Waveform_type); 
		
		// send data packet with waveform
		fCallReturn = SendDataPacket((*chanSetPtrPtr)->srcVChan, dataPacket, 0);
		if (fCallReturn) goto SendDataError;
		
		// next AI channel
		chIdx++;
	}
	
	OKfree(readBuffer);
	return 0;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	free(errMsg);
	ClearDAQmxTasks(dev); 
	OKfree(readBuffer); 
	return 0;
	
MemError:
	ClearDAQmxTasks(dev);
	TaskControlIterationDone(dev->taskController, AIDAQmxTaskDataAvailable_CB_Err_OutOfMemory, "Error: Out of memory", FALSE);  
	OKfree(readBuffer);
	return 0;
	
SendDataError:
	ClearDAQmxTasks(dev);
	TaskControlIterationDone(dev->taskController, AIDAQmxTaskDone_CB_Err_SendDataPacket, fCallReturn->errorInfo, FALSE);  
	discard_FCallReturn_type(&fCallReturn);
	OKfree(readBuffer);
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition or generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK AIDAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
#define AIDAQmxTaskDone_CB_Err_OutOfMemory		-1
#define AIDAQmxTaskDone_CB_Err_SendDataPacket	-2
	
	Dev_type*			dev 			= callbackData;
	uInt32				nSamples;									// number of samples per channel in the AI buffer
	float64*    		readBuffer		= NULL;						// temporary buffer to place data into       
	uInt32				nAI;
	int					error			= 0;
	int					nRead;
	ChanSet_type**		chanSetPtrPtr;
	size_t				nItems			= ListNumItems(dev->AITaskSet->chanSet);
	FCallReturn_type*	fCallReturn; 

	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	if (status < 0) goto DAQmxError;
	
	// get all read samples from the input buffer 
	DAQmxGetReadAttribute(taskHandle, DAQmx_Read_AvailSampPerChan, &nSamples); 
	
	// if there are no samples left in the buffer, send NULL data packet and stop here, otherwise read them out
	if (!nSamples) {
		for (size_t i = 1; i <= nItems; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			// include only channels for which HW-timing is required
			if ((*chanSetPtrPtr)->onDemand) continue;
			
			// send NULL packet to signal end of data transmission
			fCallReturn = SendDataPacket((*chanSetPtrPtr)->srcVChan, NULL, 0);
			if (fCallReturn) goto SendDataError;
		}
		
		// Task Controller iteration is complete if all DAQmx Tasks are complete
		if (DAQmxTasksDone(dev)) {
			// stop explicitly the Task
			DAQmxStopTask(taskHandle);
			TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		}
		
		return 0;
	}
	
	// allocate memory for samples
	DAQmxGetTaskAttribute(taskHandle, DAQmx_Task_NumChans, &nAI);
	readBuffer = malloc(nSamples * nAI * sizeof(float64));
	if (!readBuffer) goto MemError;
	
	// read remaining samples from the AI buffer
	DAQmxErrChk(DAQmxReadAnalogF64(taskHandle, -1, dev->AITaskSet->timeout, DAQmx_Val_GroupByChannel , readBuffer, nSamples * nAI, &nRead, NULL));
	
	// forward data to Source VChans
	size_t				chIdx			= 0;
	double*				waveformData;
	DataPacket_type*	dataPacket;
	Waveform_type*		waveform;
	for (size_t i = 1; i <= nItems; i++) {
		chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		// include only channels for which HW-timing is required
		if ((*chanSetPtrPtr)->onDemand) continue;
		
		// create waveform
		waveformData = malloc(nSamples * sizeof(double));
		if (!waveformData) goto MemError;
		memcpy(waveformData, readBuffer + chIdx * nSamples, nSamples * sizeof(double));
		waveform = init_Waveform_type(Waveform_Double, *dev->AITaskSet->timing->refSampleRate, nSamples, waveformData);
		dataPacket = init_DataPacket_type(DL_Waveform_Double, waveform, (DiscardPacketDataFptr_type) discard_Waveform_type);  
		
		// send data packet with waveform
		fCallReturn = SendDataPacket((*chanSetPtrPtr)->srcVChan, dataPacket, 0);
		if (fCallReturn) goto SendDataError;
		// send NULL packet to signal end of data transmission
		fCallReturn = SendDataPacket((*chanSetPtrPtr)->srcVChan, NULL, 0);
		if (fCallReturn) goto SendDataError;
		
		// next AI channel
		chIdx++;
	}
	
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	if (DAQmxTasksDone(dev)) {
		// stop explicitly the Task
		DAQmxStopTask(taskHandle);
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
	}
	
	OKfree(readBuffer);
	return 0;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	free(errMsg);
	ClearDAQmxTasks(dev); 
	OKfree(readBuffer); 
	return 0;
	
MemError:
	ClearDAQmxTasks(dev);
	TaskControlIterationDone(dev->taskController, AIDAQmxTaskDone_CB_Err_OutOfMemory, "Error: Out of memory", FALSE);  
	OKfree(readBuffer);
	return 0;
	
SendDataError:
	ClearDAQmxTasks(dev);
	TaskControlIterationDone(dev->taskController, AIDAQmxTaskDone_CB_Err_SendDataPacket, fCallReturn->errorInfo, FALSE);  
	discard_FCallReturn_type(&fCallReturn);
	OKfree(readBuffer);
	return 0;
}

int32 CVICALLBACK AODAQmxTaskDataRequest_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	Dev_type*			dev 			= callbackData;
	FCallReturn_type*	fCallReturn		= NULL; 
	
	fCallReturn = WriteAODAQmx(dev);
		
	if (fCallReturn) { 
		TaskControlIterationDone(dev->taskController, fCallReturn->retVal, fCallReturn->errorInfo, FALSE);
		discard_FCallReturn_type(&fCallReturn);
	}
		
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition of generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK AODAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	Dev_type*	dev = callbackData;
	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	if (status < 0) goto DAQmxError;
	
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	if (DAQmxTasksDone(dev))
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
	return 0;
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	TaskControlIterationDone(dev->taskController, status, errMsg, FALSE);
	free(errMsg);
	ClearDAQmxTasks(dev); 
	return 0;
}

int32 CVICALLBACK DIDAQmxTaskDataAvailable_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	Dev_type*	dev = callbackData;
	
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition of generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK DIDAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	Dev_type*	dev = callbackData;
	
}

int32 CVICALLBACK DODAQmxTaskDataRequest_CB	(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	Dev_type*	dev = callbackData;
	
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition of generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK DODAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	Dev_type*	dev = callbackData;
	
}


// Called only if a running task encounters an error or stops by itself in case of a finite acquisition or generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK CODAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
#define CODAQmxTaskDone_CB_Err_OutOfMemory		-1
#define CODAQmxTaskDone_CB_Err_SendDataPacket	-2
	
	Dev_type*			dev 			= callbackData;
	uInt32				nAI;
	int					error			= 0;
	int					nRead;
	ChanSet_type**		chanSetPtrPtr;
	FCallReturn_type*	fCallReturn; 

	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	if (status < 0) goto DAQmxError;
	
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	if (DAQmxTasksDone(dev)) {
		// stop explicitly the Task
		DAQmxStopTask(taskHandle);
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
	}
	
	
	return 0;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	free(errMsg);
	ClearDAQmxTasks(dev); 
	return 0;
	
MemError:
	ClearDAQmxTasks(dev);
	TaskControlIterationDone(dev->taskController, CODAQmxTaskDone_CB_Err_OutOfMemory, "Error: Out of memory", FALSE);  
	return 0;
	
SendDataError:
	ClearDAQmxTasks(dev);
	TaskControlIterationDone(dev->taskController, CODAQmxTaskDone_CB_Err_SendDataPacket, fCallReturn->errorInfo, FALSE);  
	discard_FCallReturn_type(&fCallReturn);
	return 0;
}

//-----------------------------------------
// NIDAQmx Device Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	// set finite/continuous Mode and dim number of repeats if neccessary
	BOOL	continuousFlag;
	GetCtrlVal(dev->devPanHndl, TaskSetPan_Mode, &continuousFlag);
	if (continuousFlag) {
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, 1);
		SetTaskControlMode(dev->taskController, TASK_CONTINUOUS);
	} else {
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, 0);
		SetTaskControlMode(dev->taskController, TASK_FINITE);
	}
	
	// wait time
	double	waitTime;
	GetCtrlVal(dev->devPanHndl, TaskSetPan_Wait, &waitTime);
	SetTaskControlIterationsWait(dev->taskController, waitTime);
	
	// repeats
	size_t nRepeat;
	GetCtrlVal(dev->devPanHndl, TaskSetPan_Repeat, &nRepeat);
	SetTaskControlIterations(dev->taskController, nRepeat);
	
	// reset current iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, 0);
	
	
	return init_FCallReturn_type(0, "", "");
}

static void	IterateTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag)
{
	Dev_type*			dev			= GetTaskControlModuleData(taskControl);
	FCallReturn_type*   fCallReturn;
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, currentIteration);
	
	// fill output buffers if any
	FillOutputBuffer(dev);
	
	// start all DAQmx tasks if output tasks have their buffers filled with data
	if (OutputBuffersFilled(dev)) {
		if ((fCallReturn = StartAllDAQmxTasks(dev))) TaskControlIterationDone(taskControl, fCallReturn->retVal, fCallReturn->errorInfo, FALSE);
		// if any of the DAQmx Tasks require a HW trigger, then signal the HW Trigger Master Task Controller that the Slave HW Triggered Task Controller is armed 
		if (GetTaskControlHWTrigger(taskControl) == TASK_SLAVE_HWTRIGGER)
			TaskControlEvent(taskControl, TASK_EVENT_HWTRIG_SLAVE_ARMED, NULL, NULL);
	}
	// otherwise do this check again whenever data is placed in the Sink VChans and start all DAQmx tasks
}

static void AbortIterationTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	Dev_type*			dev			= GetTaskControlModuleData(taskControl);
	FCallReturn_type*	fCallReturn;
	fCallReturn = StopDAQmxTasks(dev);
	
	// send NULL data packets to AI channels used in the DAQmx task
	if (dev->AITaskSet) {
		size_t				nItems			= ListNumItems(dev->AITaskSet->chanSet); 
		ChanSet_type**		chanSetPtrPtr; 
		for (size_t i = 1; i <= nItems; i++) { 
			chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			// include only channels for which HW-timing is required
			if ((*chanSetPtrPtr)->onDemand) continue;
			// send NULL packet to signal end of data transmission
			SendDataPacket((*chanSetPtrPtr)->srcVChan, NULL, 0);
		}
	}
	
	TaskControlIterationDone(taskControl, fCallReturn->retVal, fCallReturn->errorInfo, FALSE); 
}

static FCallReturn_type* StartTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	return ConfigDAQmxDevice(dev);
}

static FCallReturn_type* DoneTC	(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, currentIteration);
	
	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* StoppedTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, currentIteration);
	
	return init_FCallReturn_type(0, "", "");
}

static void	DimTC (TaskControl_type* taskControl, BOOL dimmed)
{
	Dev_type*			dev			= GetTaskControlModuleData(taskControl);
	ChanSet_type**		chanSetPtrPtr;
	size_t				nChans;	
	int					panHndl;
	
	// device panel
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IO, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IOMode, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IOType, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_PhysChan, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Mode, ATTR_DIMMED, dimmed);
	if (GetTaskControlMode(dev->taskController) == TASK_FINITE)
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Wait, ATTR_DIMMED, dimmed);
	// AI
	if (dev->AITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->AITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtrPtr)->chanPanHndl, ATTR_DIMMED, dimmed); 
		}
		// settings panel
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_SettingsTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, dimmed);
		// timing panel
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TimingTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, dimmed);
		// trigger panel
		int 	trigPanHndl;
		int		nTabs;
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TriggerTabIdx, &trigPanHndl);
		GetNumTabPages(trigPanHndl, Trig_TrigSet, &nTabs);
		for (size_t i = 0; i < nTabs; i++) {
			GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, i, &panHndl);
			SetPanelAttribute(panHndl, ATTR_DIMMED, dimmed);
		}
	}
	// AO
	if (dev->AOTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->AOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtrPtr)->chanPanHndl, ATTR_DIMMED, dimmed); 
		}
		// settings panel
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_SettingsTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, dimmed);
		// timing panel
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TimingTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, dimmed);
		// trigger panel
		int 	trigPanHndl;
		int		nTabs;
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, AIAOTskSet_Tab, DAQmxAIAOTskSet_TriggerTabIdx, &trigPanHndl);
		GetNumTabPages(trigPanHndl, Trig_TrigSet, &nTabs);
		for (size_t i = 0; i < nTabs; i++) {
			GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, i, &panHndl);
			SetPanelAttribute(panHndl, ATTR_DIMMED, dimmed);
		}
	}
	// DI
	if (dev->DITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->DITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtrPtr)->chanPanHndl, ATTR_DIMMED, dimmed); 
		}
		
		// dim/undim rest of panels
	}
	// DO
	if (dev->DOTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->DOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtrPtr)->chanPanHndl, ATTR_DIMMED, dimmed); 
		}
		
		// dim/undim rest of panels
	}
	// CI
	if (dev->CITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->CITaskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			SetPanelAttribute((*chanSetPtrPtr)->chanPanHndl, ATTR_DIMMED, dimmed); 
		}
		
		// dim/undim rest of panels
	}
	// CO
	if (dev->COTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->COTaskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtrPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
			SetPanelAttribute((*chanSetPtrPtr)->chanPanHndl, ATTR_DIMMED, dimmed); 
		}
		
		// dim/undim rest of panels
	}
}

static FCallReturn_type* ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, 0);
	
	return init_FCallReturn_type(0, "", "");
}

static void	ErrorTC (TaskControl_type* taskControl, char* errorMsg)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	DLMsg(errorMsg, 1);
	
}

static FCallReturn_type* DataReceivedTC	(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag)
{
#define DataReceivedTC_Err_OutOfMem					-1
#define DataReceivedTC_Err_OnDemandNElements		-2
#define DataReceivedTC_Err_BufferFillFailed			-3
	
	
	Dev_type*			dev					= GetTaskControlModuleData(taskControl);
	ChanSet_type*		chan				= GetPtrToVChanOwner((VChan_type*)sinkVChan);
	FCallReturn_type*	fCallReturn			= NULL;
	int					error;
	DataPacket_type**	dataPackets			= NULL;
	double* 			doubleDataPtr		= NULL;
	size_t				nPackets;
	size_t				nElem;
	char*				errMsg				= NULL;
	char*				VChanName			= GetVChanName((VChan_type*)sinkVChan);
	
	switch(taskState) {
			
		case TASK_STATE_UNCONFIGURED:			
		case TASK_STATE_CONFIGURED:						
		case TASK_STATE_INITIAL:							
		case TASK_STATE_IDLE:
		case TASK_STATE_STOPPING:
		case TASK_STATE_DONE:
			
			if (chan->onDemand) {
				// for on demand software output
				TaskHandle						taskHndl		= 0;
				ChanSet_AIAO_Voltage_type*		aoVoltageChan	= (ChanSet_AIAO_Voltage_type*) chan;
				ChanSet_AIAO_Current_type*		aoCurrentChan	= (ChanSet_AIAO_Current_type*) chan;
				
				// get all available data packets
				if ((fCallReturn = GetAllDataPackets(sinkVChan, &dataPackets, &nPackets))) goto Error;
				
				// create DAQmx task and channel
				switch (chan->chanType) {
						
						case Chan_AO_Voltage:
							
							// create task
							DAQmxErrChk(DAQmxCreateTask("On demand AO voltage task", &taskHndl));
							// create DAQmx channel  
							DAQmxErrChk(DAQmxCreateAOVoltageChan(taskHndl, chan->name, "", aoVoltageChan->Vmin , aoVoltageChan->Vmax, DAQmx_Val_Volts, ""));  
							break;
							
						case Chan_AO_Current:
							
							// create task
							DAQmxErrChk(DAQmxCreateTask("On demand AO current task", &taskHndl));
							// create DAQmx channel  
							DAQmxErrChk(DAQmxCreateAOCurrentChan(taskHndl, chan->name, "", aoCurrentChan->Imin , aoCurrentChan->Imax, DAQmx_Val_Amps, ""));  
							break;
				}
				
				// start DAQmx task
				DAQmxErrChk(DAQmxStartTask(taskHndl));

				// write one sample at a time
				switch (chan->chanType) {
						
					case Chan_AO_Voltage:
					case Chan_AO_Current:
						
						DLDataTypes			dataPacketType;
						void*				dataPacketDataPtr;
						float*				floatDataPtr;
						double				doubleData;
						for (size_t i = 0; i < nPackets; i++) {
							dataPacketDataPtr = GetDataPacketDataPtr(dataPackets[i], &dataPacketType);
							switch (dataPacketType) {
								case DL_Waveform_Double:
									doubleDataPtr = GetWaveformDataPtr(dataPacketDataPtr, &nElem);
									for (size_t j = 0; j < nElem; j++) {							  // update AO one sample at a time from the provided waveform
										DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &doubleDataPtr[j], NULL, NULL));
										if (*abortFlag) break;
									}
									break;
									
								case DL_Float:
									doubleData = (double)(*(float*)dataPacketDataPtr);
									DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &doubleData, NULL, NULL));
									break;
									
								case DL_Double:
									DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, (float64*)dataPacketDataPtr, NULL, NULL));
									break;
							}
							
							ReleaseDataPacket(&dataPackets[i]);
						}
						break;
				}
				
				OKfree(dataPackets);				
				DAQmxClearTask(taskHndl);
			}
			
			break;
		
		case TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES:
		case TASK_STATE_RUNNING:
		case TASK_STATE_RUNNING_WAITING_ITERATION:
			
			// write data to output buffers
			FillOutputBuffer(dev);
				
			// start all DAQmx tasks if output tasks have their buffers filled with data
			if (OutputBuffersFilled(dev)) {
				if ((fCallReturn = StartAllDAQmxTasks(dev))) TaskControlIterationDone(taskControl, fCallReturn->retVal, fCallReturn->errorInfo, FALSE);
				// if any of the DAQmx Tasks require a HW trigger, then signal the HW Trigger Master Task Controller that the Slave HW Triggered Task Controller is armed 
				if (GetTaskControlHWTrigger(taskControl) == TASK_SLAVE_HWTRIGGER)
					TaskControlEvent(taskControl, TASK_EVENT_HWTRIG_SLAVE_ARMED, NULL, NULL);
			}
				
				
			break;
			
		case TASK_STATE_ERROR:
			
			ReleaseAllDataPackets(sinkVChan);
			
			break;
	}
	
	return init_FCallReturn_type(0, "", "");
	
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "DataReceivedTC", errMsg);
	OKfree(errMsg);
	OKfree(VChanName);
	return fCallReturn;
	
Error:
	
	OKfree(VChanName);
	OKfree(errMsg);
	return fCallReturn;
}

static FCallReturn_type* ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}





