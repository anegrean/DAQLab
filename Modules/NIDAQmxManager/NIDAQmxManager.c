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
#include <limits.h>
#include "DAQLabModule.h"
#include "NIDAQmxManager.h"
#include "UI_NIDAQmxManager.h"
#include <nidaqmx.h>
#include "daqmxioctrl.h"
#include "DataTypes.h"
#include "HWTriggering.h"
#include "utility.h"

//========================================================================================================================================================================================================
// Constants

	// NIDAQmxManager UI resource 
#define MOD_NIDAQmxManager_UI 							"./Modules/NIDAQmxManager/UI_NIDAQmxManager.uir"	
	// Out of memory message
#define OutOfMemoryMsg									"Error: Out of memory.\n\n"
	//----------------------------
	// DAQmx Task default settings
	//----------------------------
	// General task settings that use hw-timing IO
#define DAQmxDefault_Task_Timeout						5.0						// in [s]
#define DAQmxDefault_Task_SampleRate					100.0					// in [Hz]
#define DAQmxDefault_Task_MeasMode						MeasFinite
#define	DAQmxDefault_Task_NSamples						100
#define	DAQmxDefault_Task_BlockSize						4096					// must be a power of 2 !
#define DAQmxDefault_Task_RefClkFreq					1e7						// in [Hz]
	// CI Frequency
#define DAQmxDefault_CI_Frequency_Task_MinFreq			2						// in [Hz]
#define DAQmxDefault_CI_Frequency_Task_MaxFreq			100						// in [Hz]
#define DAQmxDefault_CI_Frequency_Task_Edge				Edge_Rising
#define DAQmxDefault_CI_Frequency_Task_MeasMethod		Meas_LowFreq1Ctr
#define DAQmxDefault_CI_Frequency_Task_MeasTime			0.001					// in [s]
#define DAQmxDefault_CI_Frequency_Task_Divisor			4
	// CO Task
#define DAQmxDefault_CO_Task_idlestate    				PulseTrainIdle_Low   	// terminal state at rest, pulses switch this to the oposite state
#define DAQmxDefault_CO_Task_timeout      				5.0            			// task timeout in [s] 
#define DAQmxDefault_CO_Task_nPulses      				1000            		// number of finite pulses 
 	// CO Frequency Task 
#define DAQmxDefault_CO_Frequency_Task_freq				1000000   				// frequency in [Hz]
#define DAQmxDefault_CO_Frequency_Task_dutycycle    	50.0					// duty cycle
#define DAQmxDefault_CO_Frequency_Task_initdelay 		0.0			    		// initial delay [s] 
	// CO Time Task
#define DAQmxDefault_CO_Time_Task_hightime				0.0005   				// the time the pulse stays high [s]
#define DAQmxDefault_CO_Time_Task_lowtime     			0.0005					// the time the pulse stays low [s]
#define DAQmxDefault_CO_Time_Task_initdelay 			0.0			    		// initial delay [s] 
	// CO Ticks Task
#define DAQmxDefault_CO_Ticks_Task_highticks			10   					// the ticks the pulse stays high [s]
#define DAQmxDefault_CO_Ticks_Task_lowticks     		2						// the ticks the pulse stays low [s]	
#define DAQmxDefault_CO_Ticks_Task_initdelayticks 		0			    		// initial delay [ticks] 
	// DAQmx task settings tab names
#define DAQmxAITaskSetTabName							"AI"
#define DAQmxAOTaskSetTabName							"AO"
#define DAQmxDITaskSetTabName							"DI"
#define DAQmxDOTaskSetTabName							"DO"
#define DAQmxCITaskSetTabName							"CI"
#define DAQmxCOTaskSetTabName							"CO"
	// DAQmx AI, AO, DI and DO task settings tab indices
#define DAQmxADTskSet_ChanTabIdx						0
#define DAQmxADTskSet_SettingsTabIdx					1
#define DAQmxADTskSet_TimingTabIdx						2
#define DAQmxADTskSet_TriggerTabIdx						3
	// DAQmx CI and CO task settings tab indices
#define DAQmxCICOTskSet_SettingsTabIdx					0
#define DAQmxCICOTskSet_TimingTabIdx					1
#define DAQmxCICOTskSet_ClkTabIdx						2  
#define DAQmxCICOTskSet_TriggerTabIdx					3
	// DAQmx error macro
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto DAQmxError; else
	// Cmt library error macro
#define CmtErrChk(functionCall) if( (error=(functionCall)) < 0 ) goto CmtError; else
	// Shared error codes
#define WriteAODAQmx_Err_DataUnderflow 					-1

//--------------------------------------------------------------------------------------
// VChan base names and HW triggers to which the DAQmx task controller name is prepended
//--------------------------------------------------------------------------------------
// Note: These names must be unique per task controller

	//----------
	// AI	   
	//----------

#define SinkVChan_AINSamples_BaseName					"AI Num. Samples IN"		// receives number of samples to acquire for finite AI
#define SourceVChan_AINSamples_BaseName					"AI Num. Samples OUT"		// sends number of samples used to acquire finite AI
#define SinkVChan_AISamplingRate_BaseName				"AI Sampling Rate IN"		// receives AI sampling rate info
#define SourceVChan_AISamplingRate_BaseName				"AI Sampling Rate OUT"		// sends AI sampling rate info
#define HWTrig_AI_BaseName								"AI Start"					// for master and slave HW triggering

	//----------
	// AO	   
	//----------

#define SinkVChan_AONSamples_BaseName					"AO Num. Samples IN"		// receives number of samples to acquire for finite AI
#define SourceVChan_AONSamples_BaseName					"AO Num. Samples OUT"		// sends number of samples used to acquire finite AI
#define SinkVChan_AOSamplingRate_BaseName				"AO Sampling Rate IN"		// receives AI sampling rate info
#define SourceVChan_AOSamplingRate_BaseName				"AO Sampling Rate OUT"		// sends AI sampling rate info
#define HWTrig_AO_BaseName								"AO Start"					// for master and slave HW triggering 

	//----------
	// DI	   
	//----------

#define SinkVChan_DINSamples_BaseName					"DI Num. Samples IN"		// receives number of samples to acquire for finite AI
#define SourceVChan_DINSamples_BaseName					"DI Num. Samples OUT"		// sends number of samples used to acquire finite AI
#define SinkVChan_DISamplingRate_BaseName				"DI Sampling Rate IN"		// receives AI sampling rate info
#define SourceVChan_DISamplingRate_BaseName				"DI Sampling Rate OUT"		// sends AI sampling rate info
#define HWTrig_DI_BaseName								"DI Start"					// for master and slave HW triggering

	//----------
	// DO	   
	//----------

#define SinkVChan_DONSamples_BaseName					"DO Num. Samples IN"		// receives number of samples to acquire for finite AI
#define SourceVChan_DONSamples_BaseName					"DO Num. Samples OUT"		// sends number of samples used to acquire finite AI
#define SinkVChan_DOSamplingRate_BaseName				"DO Sampling Rate IN"		// receives AI sampling rate info
#define SourceVChan_DOSamplingRate_BaseName				"DO Sampling Rate OUT"		// sends AI sampling rate info
#define HWTrig_DO_BaseName								"DO Start"					// for master and slave HW triggering
	//----------
	// CO	   
	//----------

#define SinkVChan_COPulseTrain_BaseName					"CO Pulse Train IN"			// receives pulse train settings
#define SourceVChan_COPulseTrain_BaseName				"CO Pulse Train OUT"		// sends pulse train settings
#define HWTrig_CO_BaseName								"CO Start"					// for master and slave HW triggering


//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
// VChan base names to which the DAQmx task controller name is prepended
//-----------------------------------------------------------------------

#define VChan_Data_Receive_Timeout						10000						// timeout in [ms] to receive data

//========================================================================================================================================================================================================
// Types
typedef struct NIDAQmxManager 	NIDAQmxManager_type; 

typedef int CVICALLBACK (*CVICtrlCBFptr_type) (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


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
	Chan_DI_Line,									// Digital input. Single Line. Multiple channels (list or range) per task allowed.
	Chan_DI_Port,									// Digital input. Single Line. Multiple channels (list or range) per task allowed.
	//-------------------------------------------------------------------------------------------------------------------------------
	// 														Digital Output
	//-------------------------------------------------------------------------------------------------------------------------------
	Chan_DO_Line,									// Digital output. Single Line. Multiple channels (list or range) per task allowed.
	Chan_DO_Port,									// Digital output. Single Port. Multiple channels (list or range) per task allowed.      
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


typedef struct Device Dev_type;

// AI
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/ai0
	ListType					supportedMeasTypes;			// List of int type. Indicates the measurement types supported by the channel.
	int							terminalCfg;				// List of bitfield terminal configurations supported by the channel.
	IORange_type* 				Vrngs;      				// Ranges for voltage input  [V]
	IORange_type* 				Irngs;      				// Ranges for current input  [A]
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} AIChannel_type;

// AO
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/ao0
	ListType					supportedOutputTypes;		// List of int type. Indicates the output types supported by the channel.
	int							terminalCfg;				// List of bitfield terminal configurations supported by the channel.
	IORange_type* 				Vrngs;      				// Ranges for voltage output  [V]
	IORange_type* 				Irngs;      				// Ranges for current output  [A]
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} AOChannel_type;

// DI Line
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/port0/line0
	ListType					sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital input.
	BOOL						sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital input physical channel.
	BOOL						changeDetectSupported;		// Indicates if the change detection timing type is supported for the digital input physical channel.
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} DILineChannel_type;

// DI Port
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/port0
	ListType					sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital input.
	BOOL						sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital input physical channel.
	BOOL						changeDetectSupported;		// Indicates if the change detection timing type is supported for the digital input physical channel.
	unsigned int				portWidth;					// Indicates in bits the width of digital input port.
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} DIPortChannel_type;

// DO Line
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/port0/line0
	BOOL						sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital output physical channel.
	ListType					sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital output.
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} DOLineChannel_type;

// DO Port
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/port0
	BOOL						sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital output physical channel.
	ListType					sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital output.
	unsigned int				portWidth;					// Indicates in bits the width of digital output port.
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} DOPortChannel_type;

// CI
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/ctr0
	ListType					supportedMeasTypes;			// Indicates the measurement types supported by the channel.
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} CIChannel_type;

// CO
typedef struct {
	char* 						physChanName;				// Physical channel name, e.g. dev1/ctr0
	ListType					supportedOutputTypes;		// Indicates the output types supported by the channel.
	BOOL						inUse;						// True if channel is in use by a Task, False otherwise
} COChannel_type;

// DAQ device parameters
typedef struct {
	char*        				name;    	   				// Device name.
	char*        				type;    	   				// Indicates the product name of the device.
	unsigned int				productNum;					// unique product number
	unsigned int				serial;       				// device serial number
	double       				AISCmaxrate;  				// Single channel AI max rate   [Hz]
	double       				AIMCmaxrate;  				// Multiple channel AI max rate [Hz]
	double       				AIminrate;    				// AI minimum rate              [Hz]
	double       				AOmaxrate;    				// AO maximum rate              [Hz]
	double       				AOminrate;    				// AO minimum rate              [Hz]
	double       				DImaxrate;    				// DI maximum rate              [Hz]
	double       				DOmaxrate;    				// DO maximum rate              [Hz]
	ListType     				AIchan;       				// List of AIChannel_type*
	ListType     				AOchan;	   					// List of AOChannel_type*
	ListType     				DIlines;      				// List of DILineChannel_type*
	ListType     				DIports;      				// List of DIPortChannel_type*
	ListType     				DOlines;      				// List of DOLineChannel_type*
	ListType     				DOports;      				// List of DOPortChannel_type*
	ListType     				CIchan;	   					// List of CIChannel_type*
	ListType     				COchan;	   					// List of COChannel_type*
	BOOL						AnalogTriggering;			// Indicates if the device supports analog triggering.
	BOOL						DigitalTriggering;			// Indicates if the device supports digital triggering.
	int							AITriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_AI_TrigUsage device attribute for bitfield info.
	int							AOTriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_AO_TrigUsage device attribute for bitfield info.
	int							DITriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_DI_TrigUsage device attribute for bitfield info.
	int							DOTriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_DO_TrigUsage device attribute for bitfield info.
	int							CITriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_CI_TrigUsage device attribute for bitfield info.
	int							COTriggerUsage;				// List of bitfields with trigger usage. See DAQmx_Dev_CO_TrigUsage device attribute for bitfield info.
	ListType					AISupportedMeasTypes;		// List of int type. Indicates the measurement types supported by the physical channels of the device for analog input.
	ListType					AOSupportedOutputTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for analog output.
	ListType					CISupportedMeasTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for counter input.
	ListType					COSupportedOutputTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for counter output.
} DevAttr_type;

// Terminal type bit fields
typedef enum {
	Terminal_Bit_None 			= 0, 								// no terminal 
	Terminal_Bit_Diff 			= DAQmx_Val_Bit_TermCfg_Diff,		// differential 
	Terminal_Bit_RSE 			= DAQmx_Val_Bit_TermCfg_RSE ,		// referenced single-ended 
	Terminal_Bit_NRSE 			= DAQmx_Val_Bit_TermCfg_NRSE,		// non-referenced single-ended
	Terminal_Bit_PseudoDiff 	= DAQmx_Val_Bit_TermCfg_PseudoDIFF	// pseudo-differential
} TerminalBitField_type;

// Terminal type
typedef enum {
	Terminal_None 				= 0, 						// no terminal 
	Terminal_Diff 				= DAQmx_Val_Diff,			// differential 
	Terminal_RSE 				= DAQmx_Val_RSE,			// referenced single-ended 
	Terminal_NRSE 				= DAQmx_Val_NRSE,			// non-referenced single-ended
	Terminal_PseudoDiff 		= DAQmx_Val_PseudoDiff		// pseudo-differential
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
	TrigWnd_Entering			= DAQmx_Val_EnteringWin,	// Entering window
	TrigWnd_Leaving				= DAQmx_Val_LeavingWin		// Leaving window
} TrigWndCond_type;											// Window trigger condition for analog triggering.

typedef enum {
	TrigSlope_Rising			=  DAQmx_Val_Rising,
	TrigSlope_Falling			=  DAQmx_Val_Falling
} TrigSlope_type;											// For analog and digital edge trigger type

typedef enum {
	SampClockEdge_Rising		=  DAQmx_Val_Rising,
	SampClockEdge_Falling   	=  DAQmx_Val_Falling
} SampClockEdge_type;										// Sampling clock active edge.

// Measurement mode
typedef enum{
	MeasFinite					= DAQmx_Val_FiniteSamps,
	MeasCont					= DAQmx_Val_ContSamps
} MeasMode_type; 

// timing structure for ADC/DAC & digital sampling tasks
typedef struct {
	MeasMode_type 				measMode;      				// Measurement mode: finite or continuous.
	double       				sampleRate;    				// Target sampling rate in [Hz].
	uInt64        				nSamples;	    			// Target number of samples to be acquired in case of a finite recording.
	uInt32						oversampling;				// Oversampling factor, used in AI. This determines the actual sampling rate and the actual number of samples to be acquired
	SinkVChan_type*				nSamplesSinkVChan;			// Used for receiving number of samples to be generated/received with each iteration of the DAQmx task controller.
															// data packets of DL_UChar, DL_UShort, DL_UInt, DL_ULong and DL_ULongLong types.
	SourceVChan_type*			nSamplesSourceVChan;		// Used for sending number of samples for finite tasks.
															// data packets of DL_ULongLong type.
	SinkVChan_type*				samplingRateSinkVChan;		// Used for receiving sampling rate info with each iteration of the DAQmx task controller.  
															// data packets of DL_Double, DL_Float types.
	SourceVChan_type*			samplingRateSourceVChan;	// Used for sending sampling rate info.
															// data packets of DL_Double type.
	uInt32        				blockSize;     				// Number of samples for reading after which callbacks are called.
	char*         				sampClkSource; 				// Sample clock source if NULL then OnboardClock is used, otherwise the given clock
	SampClockEdge_type 			sampClkEdge;   				// Sample clock active edge.
	char*         				refClkSource;  				// Reference clock source used to sync internal clock, if NULL internal clock has no reference clock. 
	double        				refClkFreq;    				// Reference clock frequency if such a clock is used.
	
	//------------------------------------
	// UI
	//------------------------------------
	int							timingPanHndl;				// Panel handle containing timing controls.
	int							settingsPanHndl;
} TaskTiming_type;

// timing structure for counter tasks
typedef struct {
	MeasMode_type 				measMode;      				// Measurement mode: finite or continuous.     
	uInt64        				nSamples;	    			// Total number of samples or pulses to be acquired/generated in case of a finite recording if device task settings is used as reference.
	uInt64*						refNSamples;				// Points to either device or VChan-based number of samples to acquire. By default points to device. 
	double       				sampleRate;    				// Sampling rate in [Hz] if device task settings is used as reference. 
	double*						refSampleRate;				// Points to either device or VChan-based sampling rate. By default points to device.    
	double        				refClkFreq;    				// Reference clock frequency if such a clock is used.
} CounterTaskTiming_type;

	
typedef struct {
	Trig_type 					trigType;					// Trigger type.
	char*     					trigSource;   				// Trigger source.
	double*						samplingRate;				// Reference to task sampling rate in [Hz]
	TrigSlope_type				slope;     					// For analog and digital edge trig type.
	double   					level; 						// For analog edge trigger.
	double    					wndTop; 	   				// For analog window trigger.
	double   					wndBttm;   					// For analog window trigger.
	TrigWndCond_type			wndTrigCond;  				// For analog window trigger.
	uInt32						nPreTrigSamples;			// For reference type trigger. Number of pre-trigger samples to acquire.
	Dev_type*					device;						// Reference to the device that owns this task trigger.
	//------------------------------------
	// UI
	//------------------------------------
	int							trigSlopeCtrlID;			// Trigger setting control copy IDs
	int							trigSourceCtrlID;			// Trigger setting control copy IDs
	int							preTrigDurationCtrlID;		// Trigger setting control copy IDs
	int							preTrigNSamplesCtrlID;		// Trigger setting control copy IDs
	int							levelCtrlID;				// Trigger setting control copy IDs
	int							windowTrigCondCtrlID;		// Trigger setting control copy IDs
	int							windowTopCtrlID;			// Trigger setting control copy IDs
	int							windowBottomCtrlID;			// Trigger setting control copy IDs
} TaskTrig_type;


typedef struct ChanSet	ChanSet_type;
typedef void (*DiscardChanSetFptr_type) (ChanSet_type** chanSet);
struct ChanSet {
	//DATA
	char*						name;						// Full physical channel name used by DAQmx, e.g. Dev1/ai1 
	Channel_type				chanType;					// Channel type.
	int							chanPanHndl;				// Panel handle where channel properties are adjusted
	TaskHandle					taskHndl;					// Only for counter type channels. Each counter requires a separate task.
	Dev_type*					device;						// reference to device owing the channel
	TaskTrig_type* 				startTrig;     				// Only for counter type channels. Task start trigger type. If NULL then there is no start trigger.
	TaskTrig_type* 				referenceTrig;     			// Only for counter type channels. Task reference trigger type. If NULL then there is no reference trigger.
	HWTrigMaster_type*			HWTrigMaster;				// Only for counter type channels.
	HWTrigSlave_type*			HWTrigSlave;				// Only for counter type channels.
	SourceVChan_type*			srcVChan;					// Source VChan assigned to input type physical channels.
	SinkVChan_type*				sinkVChan;					// Sink VChan assigned to output type physical channels.
	BOOL						onDemand;					// If TRUE, the channel is updated/read out using software-timing.
	double						ScaleMax;					// Maximum value in the channel's measurement units used to rescale an input signal when data type is other than double to 
															// the range of the chosen data type.	
	double						ScaleMin;					// Minimum value in the channel's measurement units used to rescale an input signal when data type is other than double to
															// the range of the chosen data type.	
	double						offset;						// Offset added to the AI signal.
	double						gain;						// Multiplication factor to be applied to the AI signal before it is converted to another data type.
	BOOL						integrateFlag;				// Used for AI. If true, then based on the AI task settings, samples are integrated, otherwise, the oversamples are discarded.
	// METHODS
	DiscardChanSetFptr_type		discardFptr;				// Function pointer to discard specific channel data that is derived from this base class.					
};

// Analog Input & Output channel type settings
typedef enum {
	AI_Double,
	AI_Float,
	AI_UInt,
	AI_UShort,
	AI_UChar
} AIDataTypes;

typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	AIChannel_type*				aiChanAttr;					// If AI channel, this points to channel attributes, otherwise this is NULL.
	AOChannel_type*				aoChanAttr;					// If AO channel, this points to channel attributes, otherwise this is NULL.
	double        				Vmax;      					// Maximum voltage [V]
	double        				Vmin;       				// Minimum voltage [V]
	Terminal_type 				terminal;   				// Terminal type 	
} ChanSet_AIAO_Voltage_type;

typedef enum {
	ShuntResistor_Internal 		= DAQmx_Val_Internal,		// Use the internal shunt resistor for current measurements.
	ShuntResistor_External 		= DAQmx_Val_External		// Use an external shunt resistor for current measurement.
} ShuntResistor_type;

typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	double        				Imax;      					// Maximum voltage [A]
	double        				Imin;       				// Minimum voltage [A]
	ShuntResistor_type			shuntResistorType;
	double						shuntResistorValue;			// If shuntResistorType = ShuntResistor_External, this gives the value of the shunt resistor in [Ohms] 
	Terminal_type 				terminal;   				// Terminal type 	
} ChanSet_AIAO_Current_type;


// Digital Line/Port Input & Digital Line/Port Output channel type settings
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	BOOL 						invert;    					// invert polarity (for ports the polarity is inverted for all lines
} ChanSet_DIDO_type;

// Counter Input for Counting Edges, channel type settings
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	BOOL 						ActiveEdge; 				// 0 - Falling, 1 - Rising
	int 						initialCount;				// value to start counting from
	enum {
		C_UP, 
		C_DOWN, 
		C_EXT
	} 							countDirection; 			// count direction, C_EXT = externally controlled
} CIEdgeChanSet_type;

// Counter Input for measuring the frequency of a digital signal
typedef enum {
	Edge_Rising					=  DAQmx_Val_Rising,
	Edge_Falling				=  DAQmx_Val_Falling
} Edge_type;

typedef enum {
	Meas_LowFreq1Ctr			= DAQmx_Val_LowFreq1Ctr,	// Use one counter that uses a constant timebase to measure the input signal.
	Meas_HighFreq2Ctr			= DAQmx_Val_HighFreq2Ctr,	// Use two counters, one of which counts pulses of the signal to measure during the specified measurement time.
	Meas_LargeRng2Ctr			= DAQmx_Val_LargeRng2Ctr    // Use one counter to divide the frequency of the input signal to create a lower-frequency signal 
						  									// that the second counter can more easily measure.
} MeasMethod_type;

typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	double        				freqMax;      				// Maximum frequency expected to be measured [Hz]
	double        				freqMin;       				// Minimum frequency expected to be measured [Hz]
	Edge_type					edgeType;					// Specifies between which edges to measure the frequency or period of the signal.
	MeasMethod_type				measMethod;					// The method used to calculate the period or frequency of the signal. 
	double						measTime;					// The length of time to measure the frequency or period of a digital signal, when measMethod is DAQmx_Val_HighFreq2Ctr.
															// Measurement accuracy increases with increased measurement time and with increased signal frequency.
															// Caution: If you measure a high-frequency signal for too long a time, the count register could roll over, resulting in an incorrect measurement. 
	unsigned int				divisor;					// The value by which to divide the input signal, when measMethod is DAQmx_Val_LargeRng2Ctr. The larger this value, the more accurate the measurement, 
															// but too large a value can cause the count register to roll over, resulting in an incorrect measurement.
	char*						freqInputTerminal;			// If other then default terminal assigned to the counter, otherwise this is NULL.
	CounterTaskTiming_type*		taskTiming;					// Task timing info.
} ChanSet_CI_Frequency_type;


// Counter Output channel type settings
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	PulseTrain_type*			pulseTrain;					// Pulse train info.
	CounterTaskTiming_type*		taskTiming;					// Task timing info.
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
} DAQmxIOMode_type;											// not used

typedef enum {
	DAQmxDigLines,
 	DAQmxDigPorts
} DAQmxDigChan_type;

// Used for continuous AO streaming
typedef struct {
	size_t        				writeblock;        			// Size of writeblock is IO_block_size
	size_t        				numchan;           			// Number of output channels
	
	float64**     				datain;            
	float64**     				databuff;
	float64*      				dataout;					// Array length is writeblock * numchan used for DAQmx write call, data is grouped by channel
	SinkVChan_type**			sinkVChans;					// Array of SinkVChan_type*
	
	size_t*       				datain_size; 
	size_t*      				databuff_size;
	
	size_t*       				idx;						// Array of pointers with index for each channel from which to continue writing data (not used yet)  
	size_t*       				datain_repeat;	 			// Number of times to repeat the an entire data packet
	size_t*      				datain_remainder;  			// Number of elements from the beginning of the data packet to still generate, 
									 						// WARNING: this does not apply when looping is ON. In this case when receiving a new data packet, a full cycle is generated before switching.
	
	BOOL*         				datain_loop;	   			// Data will be looped regardless of repeat value until new data packet is provided
	BOOL*						nullPacketReceived;			// Array of BOOL equal to the number of AO channels. If TRUE, then the AO channel received a NULL packet and it will keep on generating
															// the last value until all AO channels have also received a NULL packet.
	int							writeBlocksLeftToWrite;		// Number of writeblocks left to write before the AO task stops. This guarantees that the last value of a given waveform is generated before the stop.
} WriteAOData_type;

typedef struct {
	size_t						nAI;						// Number of AI channels used in the task.
	uInt32*						nIntBuffElem;				// Array of number of elements within each buffer that must be still integrated with incoming data.
															// This number cannot be larger than the number of elements contained within one integration block.
	float64**					intBuffers;					// Array of integration buffers, each buffer having a maximum size equal to the readblock + number of elements within one integration block. 
} ReadAIData_type;

// Used for continuous DO streaming
typedef struct {
	size_t        				writeblock;        			// Size of writeblock is IO_block_size
	size_t        				numchan;           			// Number of output channels
	
	uInt32**     				datain;            
	uInt32**     				databuff;
	uInt32*      				dataout;					// Array length is writeblock * numchan used for DAQmx write call, data is grouped by channel
	SinkVChan_type**			sinkVChans;					// Array of SinkVChan_type*
	
	size_t*       				datain_size; 
	size_t*      				databuff_size;
	
	size_t*       				idx;						// Array of pointers with index for each channel from which to continue writing data (not used yet)  
	size_t*       				datain_repeat;	 			// Number of times to repeat the an entire data packet
	size_t*      				datain_remainder;  			// Number of elements from the beginning of the data packet to still generate, 
									 						// WARNING: this does not apply when looping is ON. In this case when receiving a new data packet, a full cycle is generated before switching.
	
	BOOL*         				datain_loop;	   			// Data will be looped regardless of repeat value until new data packet is provided
} WriteDOData_type;

// AI, AO, DI & DO DAQmx task settings
typedef struct {
	Dev_type*					dev;						// Reference to DAQmx device owing this data structure.
	int							panHndl;					// Panel handle to task settings.
	TaskHandle					taskHndl;					// DAQmx task handle for hw-timed AI or AO.
	ListType 					chanSet;     				// Channel settings. Of ChanSet_type*
	double        				timeout;       				// Task timeout [s]
	TaskTrig_type* 				startTrig;     				// Task start trigger type. If NULL then there is no start trigger.
	TaskTrig_type* 				referenceTrig;     			// Task reference trigger type. If NULL then there is no reference trigger.
	TaskTiming_type*			timing;						// Task timing info
	HWTrigMaster_type*			HWTrigMaster;				// For establishing a task start HW-trigger dependency, this being a master.
	HWTrigSlave_type*			HWTrigSlave;				// For establishing a task start HW-trigger dependency, this being a slave.
	WriteAOData_type*			writeAOData;				// Used for continuous AO streaming. 
	ReadAIData_type*			readAIData;					// Used for processing of incoming AI data.
	WriteDOData_type*			writeDOData;				// Used for continuous DO streaming.                         
} ADTaskSet_type;

// CI
typedef struct {
	// DAQmx task handles are specified per counter channel within chanTaskSet
	int							panHndl;					// Panel handle to task settings.
	TaskHandle					taskHndl;					// DAQmx task handle for hw-timed CI
	ListType 					chanTaskSet;   				// Channel and task settings. Of ChanSet_type*
	double        				timeout;       				// Task timeout [s]
} CITaskSet_type;

// CO
typedef struct {
	// DAQmx task handles are specified per counter channel chanTaskSet
	int							panHndl;					// Panel handle to task settings.
	TaskHandle					taskHndl;					// DAQmx task handle for hw-timed CO.
	ListType 					chanTaskSet;     			// Channel and task settings. Of ChanSet_CO_type*
	double        				timeout;       				// Task timeout [s]
} COTaskSet_type;

// DAQ Task definition
struct Device {
	NIDAQmxManager_type*		niDAQModule;				// Reference to the module that owns the DAQmx device. 
	int							devPanHndl;					// Panel handle to DAQmx task settings.
	DevAttr_type*				attr;						// Device to be used during IO task
	TaskControl_type*   		taskController;				// Task Controller for the DAQmx module
	ADTaskSet_type*				AITaskSet;					// AI task settings
	ADTaskSet_type*				AOTaskSet;					// AO task settings
	ADTaskSet_type*				DITaskSet;					// DI task settings
	ADTaskSet_type*				DOTaskSet;					// DO task settings
	CITaskSet_type*				CITaskSet;					// CI task settings
	COTaskSet_type*				COTaskSet;					// CO task settings
	CmtTSVHandle				nActiveTasks;				// thread safe variable with the sum of all active tasks on the DAQ device, variable of int type
};

//========================================================================================================================================================================================================
// Module implementation

struct NIDAQmxManager {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 			baseClass;
	
	// DATA
	
	ListType            		DAQmxDevices;   			// list of Dev_type* for each DAQmx device 
	
		//-------------------------
		// UI
		//-------------------------
	
	int							mainPanHndl;				// NI DAQ manager main panel
	int							taskSetPanHndl;
	int							devListPanHndl;
	int							menuBarHndl;
	int							deleteDevMenuItemID;
	
	
	// METHODS 
	
};

//========================================================================================================================================================================================================
// Static global variables

static int	 currDev = -1;        // currently selected device from the DAQ table

//========================================================================================================================================================================================================
// Static functions

//---------------------------------------------
// DAQmx Module management
//---------------------------------------------

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl);

static int 							LoadCfg							 		(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement); 

static int							SaveCfg									(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement);

static int 							SaveDeviceCfg 							(Dev_type* dev, CAObjHandle  xmlDOM, ActiveXMLObj_IXMLDOMElement_ NIDAQDeviceXMLElement); 

static int 							SaveAITaskCfg 							(ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement); 

static int 							SaveAOTaskCfg 							(ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement);

static int 							SaveDITaskCfg 							(ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement); 

static int 							SaveDOTaskCfg 							(ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement);

static int 							SaveCITaskCfg 							(CITaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement); 

static int 							SaveCOTaskCfg 							(COTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement); 

static int 							SaveTaskTrigCfg 						(TaskTrig_type* taskTrig, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ TriggerXMLElement);

static int 							SaveChannelCfg 							(ChanSet_type* chanSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ ChannelXMLElement);

static int 							DisplayPanels							(DAQLabModule_type* mod, BOOL visibleFlag); 

//---------------------------------------------
// Device and Channel data structure management
//---------------------------------------------

	// device data structure
static Dev_type* 					init_Dev_type 							(NIDAQmxManager_type* niDAQModule, DevAttr_type* attr, char taskControllerName[]);
static void 						discard_Dev_type						(Dev_type** a);

	// device detection
static int 							init_DevList 							(ListType devlist, int panHndl, int tableCtrl);
static void 						empty_DevList 							(ListType devList); 

	// device attributes data structure
static DevAttr_type* 				init_DevAttr_type						(void); 
static void 						discard_DevAttr_type					(DevAttr_type** a);
static DevAttr_type* 				copy_DevAttr_type						(DevAttr_type* attr);

	// AI channel data structure
static AIChannel_type* 				init_AIChannel_type						(void); 
static void 						discard_AIChannel_type					(AIChannel_type** a);
int CVICALLBACK 					DisposeAIChannelList	 				(size_t index, void* ptrToItem, void* callbackData);
static AIChannel_type* 				copy_AIChannel_type						(AIChannel_type* channel);
static ListType						copy_AIChannelList						(ListType chanList);
static AIChannel_type*				GetAIChannel							(Dev_type* dev, char physChanName[]);

	// AO channel data structure
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
static void							init_ChanSet_type						(Dev_type* dev, ChanSet_type* chanSet, char physChanName[], Channel_type chanType, DiscardChanSetFptr_type discardFptr);
static void							discard_ChanSet_type					(ChanSet_type** a);

	// AI and AO Voltage
static ChanSet_type* 				init_ChanSet_AIAO_Voltage_type 			(Dev_type* dev, char physChanName[], Channel_type chanType, AIChannel_type* aiChanAttrPtr, AOChannel_type* aoChanAttrPtr);
static void							discard_ChanSet_AIAO_Voltage_type		(ChanSet_type** a);
static int 							ChanSetAIAOVoltage_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// DI and DO 
static ChanSet_DIDO_type*			init_ChanSet_DIDO_type					(Dev_type* dev, char physChanName[], Channel_type chanType);
static void							discard_ChanSet_DIDO_type				(ChanSet_type** a);
static int 							ChanSetLineDO_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							ChanSetPortDO_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
int 								ReadDirectDigitalOut					(char* chan, uInt32* data);																	// <---------- check & cleanup!!!
int 								DirectDigitalOut 						(char* chan, uInt32 data, BOOL invert, double timeout);										// <---------- check & cleanup!!!
void 								SetPortControls							(int panel, uInt32 data);																	// <---------- check & cleanup!!!

	// CI Frequency
static ChanSet_type* 				init_ChanSet_CI_Frequency_type 			(Dev_type* dev, char physChanName[]);
static void 						discard_ChanSet_CI_Frequency_type 		(ChanSet_type** a);
static int 							ChanSetCIEdge_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);   	// <---------- check & cleanup!!!

	// CO
static ChanSet_type* 				init_ChanSet_CO_type 					(Dev_type* dev, char physChanName[], PulseTrainTimingTypes pulseTrainType);
static void 						discard_ChanSet_CO_type 				(ChanSet_type** a);
static int 							ChanSetCO_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);	// <---------- check & cleanup!!!

	// channels & property lists
static ListType						GetPhysChanPropertyList					(char chanName[], int chanProperty);

//-------------------
// Channel management
//-------------------
static void							PopulateChannels						(Dev_type* dev);
static int							AddDAQmxChannel							(Dev_type* dev, DAQmxIO_type ioVal, DAQmxIOMode_type ioMode, int ioType, char chName[], char** errorInfo);
static int 							RemoveDAQmxAIChannel_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int 							RemoveDAQmxAOChannel_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							RemoveDAQmxDIChannel_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 	// <---- add functionality!
static int 							RemoveDAQmxDOChannel_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int 							RemoveDAQmxCIChannel_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int 			 				RemoveDAQmxCOChannel_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

	// IO mode/type
static ListType						GetSupportedIOTypes						(char devName[], int IOType);
static void 						PopulateIOMode 							(Dev_type* dev, int panHndl, int controlID, int ioVal);
static void 						PopulateIOType 							(Dev_type* dev, int panHndl, int controlID, int ioVal, int ioMode); 

	// Ranges
static IORange_type* 				init_IORange_type						(void);
static void 						discard_IORange_type					(IORange_type** IORangePtr);
static IORange_type*				copy_IORange_type						(IORange_type* IOrange);
static IORange_type*				GetIORanges								(char devName[], int rangeType);

//--------------------
// DAQmx task triggers
//--------------------
	// task trigger
static TaskTrig_type*				init_TaskTrig_type						(Dev_type* dev, double* samplingRate);
static void							discard_TaskTrig_type					(TaskTrig_type** taskTrigPtr);

//------------------
// DAQmx task timing
//------------------
	// for ADC/DAC sample generation
static TaskTiming_type*				init_TaskTiming_type					(void);
static void							discard_TaskTiming_type					(TaskTiming_type** taskTimingPtr);

	// for counter tasks
static CounterTaskTiming_type*		init_CounterTaskTiming_type				(void);									// <------- check if required
static void							discard_CounterTaskTiming_type			(TaskTiming_type** taskTimingPtr);		// <------- check if required

//--------------------
// DAQmx task settings
//--------------------
	// AI, AO, DI & DO task settings
static ADTaskSet_type*				init_ADTaskSet_type						(Dev_type* dev);
static void							discard_ADTaskSet_type					(ADTaskSet_type** taskSetPtr);

	// creates a new AD task settings structure UI
static void							newUI_ADTaskSet 						(ADTaskSet_type* tskSet, char taskSettingsTabName[], CVICtrlCBFptr_type removeDAQmxChanCBFptr, int taskTriggerUsage, 
																			 char sinkVChanNSamplesBaseName[], char sourceVChanNSamplesBaseName[], char sinkVChanSamplingRateBaseName[], 
																			 char sourceVChanSamplingRateBaseName[], char HWTrigBaseName[]);
	// discards an AD task settings structire from the UI and framework
static void							discardUI_ADTaskSet 					(ADTaskSet_type** tskSetPtr); 

static int 							ADTaskSettings_CB			 			(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							ADTimingSettings_CB			 			(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 

	// AI reading structure for processing incoming data (integration, etc.)
static ReadAIData_type*				init_ReadAIData_type					(Dev_type* dev);
static void							discard_ReadAIData_type					(ReadAIData_type** readAIPtr);

	// AO continuous streaming data structure
static WriteAOData_type* 			init_WriteAOData_type					(Dev_type* dev);
static void							discard_WriteAOData_type				(WriteAOData_type** writeDataPtr);

	// DO continuous streaming data structure
static WriteDOData_type* 			init_WriteDOData_type					(Dev_type* dev);
static void							discard_WriteDOData_type				(WriteDOData_type** writeDataPtr);

	// DI and DO task settings
static int 							DO_Timing_TaskSet_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// CI task settings
static CITaskSet_type*				init_CITaskSet_type						(void);
static void							discard_CITaskSet_type					(CITaskSet_type** a);

	// CO task settings
static COTaskSet_type*				init_COTaskSet_type						(void);
static void							discard_COTaskSet_type					(COTaskSet_type** a);
static int CVICALLBACK 				Chan_CO_Pulse_Frequency_CB				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				Chan_CO_Pulse_Time_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				Chan_CO_Pulse_Ticks_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				Chan_CO_Clk_CB							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 				Chan_CO_Trig_CB							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// adjustment of the DAQmx task settings panel
static int 							DAQmxDevTaskSet_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

//-----------------------
// DAQmx tasks management
//-----------------------
static int							ConfigDAQmxDevice						(Dev_type* dev, char** errorInfo); 	 // configures all tasks on the device
static int							ConfigDAQmxAITask 						(Dev_type* dev, char** errorInfo);
static int							ConfigDAQmxAOTask 						(Dev_type* dev, char** errorInfo);
static int							ConfigDAQmxDITask 						(Dev_type* dev, char** errorInfo);
static int							ConfigDAQmxDOTask 						(Dev_type* dev, char** errorInfo);
static int							ConfigDAQmxCITask 						(Dev_type* dev, char** errorInfo);
static int							ConfigDAQmxCOTask 						(Dev_type* dev, char** errorInfo);
static void 						DisplayLastDAQmxLibraryError 			(void);
	// Clears all DAQmx Tasks defined for the device
static int 							ClearDAQmxTasks 						(Dev_type* dev); 
	// Stops all DAQmx Tasks defined for the device
static int							StopDAQmxTasks 							(Dev_type* dev, char** errorInfo); 
	// Starts DAQmx Tasks defined for the device
static int							StartAIDAQmxTask						(Dev_type* dev, char** errorInfo);
static int CVICALLBACK 				StartAIDAQmxTask_CB 					(void *functionData);
static int							StartAODAQmxTask						(Dev_type* dev, char** errorInfo);
static int CVICALLBACK 				StartAODAQmxTask_CB 					(void *functionData);
static int							StartDIDAQmxTask						(Dev_type* dev, char** errorInfo);
static int CVICALLBACK 				StartDIDAQmxTask_CB 					(void *functionData);
static int							StartDODAQmxTask						(Dev_type* dev, char** errorInfo);
static int CVICALLBACK 				StartDODAQmxTask_CB 					(void *functionData);
static int							StartCIDAQmxTasks						(Dev_type* dev, char** errorInfo);
static int CVICALLBACK 				StartCIDAQmxTasks_CB 					(void *functionData);
static int							StartCODAQmxTasks						(Dev_type* dev, char** errorInfo);
static int CVICALLBACK 				StartCODAQmxTasks_CB 					(void *functionData);

//---------------------
// DAQmx task callbacks
//---------------------
	// AI
static int 							SendAIBufferData 						(Dev_type* dev, ChanSet_type* AIChSet, size_t chIdx, int nRead, float64* AIReadBuffer, char** errorInfo); 
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
static int				 			WriteAODAQmx 							(Dev_type* dev, char** errorInfo); 

	// DO
//FCallReturn_type*   				WriteDODAQmx							(Dev_type* dev); 					// <------------------ cleanup!!!

//--------------------------------------------
// Various DAQmx module managemement functions
//--------------------------------------------

static char* 						substr									(const char* token, char** idxstart);

static BOOL							ValidTaskControllerName					(char name[], void* dataPtr);

//---------------------------
// DAQmx device management
//---------------------------

static int CVICALLBACK 				MainPan_CB								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			ManageDevPan_CB 						(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

int CVICALLBACK 					ManageDevices_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			DeleteDev_CB 							(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void 						UnregisterDeviceFromFramework 			(Dev_type* dev);

//---------------------------
// Trigger settings callbacks
//---------------------------

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

//----------------------------------------------------
// VChan Callbacks
//----------------------------------------------------

	//-----------------------------------
	// VChan connect/disconnect callbacks
	//-----------------------------------

	//----------------------
	// AI, AO, DI & DO Tasks
	//----------------------

	// receiving number of samples
static void							ADNSamplesSinkVChan_Connected			(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							ADNSamplesSinkVChan_Disconnected		(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);
	// sending number of samples
static void							ADNSamplesSourceVChan_Connected			(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
	// receiving sampling rate
static void							ADSamplingRateSinkVChan_Connected		(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							ADSamplingRateSinkVChan_Disconnected	(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);
	// sending sampling rate
static void							ADSamplingRateSourceVChan_Connected		(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);

	//----------------
	// CO Task
	//----------------

	// receiving CO pulse train info
static void							COPulseTrainSinkVChan_Connected			(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
static void							COPulseTrainSinkVChan_Disconnected		(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);
	// sending CO pulse train info
static void							COPulseTrainSourceVChan_Connected		(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);

	//------------------------------------------------------------------- 
	// VChan receiving data callbacks when task controller is not active
	//-------------------------------------------------------------------


	// AI, AO, Di & DO 
static int							ADNSamples_DataReceivedTC				(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);
static int							ADSamplingRate_DataReceivedTC			(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);
	// AO
static int							AO_DataReceivedTC						(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);
	// DO
static int							DO_DataReceivedTC						(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);
	// CO
static int				 			CO_DataReceivedTC						(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);



//---------------------------------------------------- 
// NIDAQmx Device Task Controller Callbacks
//---------------------------------------------------- 

static int							ConfigureTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							UnconfigureTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static void							IterateTC								(TaskControl_type* taskControl, BOOL const* abortIterationFlag);

static void							AbortIterationTC						(TaskControl_type* taskControl, BOOL const* abortFlag);

static int							StartTC									(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							DoneTC									(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							StoppedTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);

static int							TaskTreeStatus 							(TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo);

static int				 			ResetTC 								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 

static void				 			ErrorTC 								(TaskControl_type* taskControl, int errorID, char errorMsg[]);

static int							ModuleEventHandler						(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo);  

//========================================================================================================================================================================================================
// Global variables

ListType	devList			= 0;		// List of DAQ devices available of DevAttr_type*. This will be updated every time init_DevList(ListType) is executed.


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
DAQLabModule_type*	initalloc_NIDAQmxManager (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	NIDAQmxManager_type* nidaq;
	
	if (!mod) {
		nidaq = malloc (sizeof(NIDAQmxManager_type));
		if (!nidaq) return NULL;
	} else
		nidaq = (NIDAQmxManager_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&nidaq->baseClass, className, instanceName, workspacePanHndl);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
		
		// METHODS
		
			// overriding methods
	nidaq->baseClass.Discard 		= discard_NIDAQmxManager;
			
	nidaq->baseClass.Load			= Load;
	
	nidaq->baseClass.LoadCfg		= LoadCfg;
	
	nidaq->baseClass.SaveCfg		= SaveCfg;
	
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
	Dev_type**				DAQDevPtr	= NULL;
	
	if (!nidaq) return;
	
	//------------------------------------------
	// discard NIDAQmxManager_type specific data
	//------------------------------------------
	
	// discard DAQmx device data
	for (size_t i = nDAQDevs; i ; i--) {
		DAQDevPtr = ListGetPtrToItem(nidaq->DAQmxDevices, i);
		UnregisterDeviceFromFramework(*DAQDevPtr); 
		discard_Dev_type(DAQDevPtr);
	}
	
	ListDispose(nidaq->DAQmxDevices);
	
	// discard UI
	if (nidaq->menuBarHndl) DiscardMenuBar (nidaq->menuBarHndl);        
	if (nidaq->mainPanHndl) {DiscardPanel(nidaq->mainPanHndl); nidaq->mainPanHndl = 0;}
	if (nidaq->taskSetPanHndl) {DiscardPanel(nidaq->taskSetPanHndl); nidaq->taskSetPanHndl = 0;}
	if (nidaq->devListPanHndl) {DiscardPanel(nidaq->devListPanHndl); nidaq->devListPanHndl = 0;}
	
	
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
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

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_  moduleElement)
{
	NIDAQmxManager_type*	NIDAQ				 	= mod;
	
	
	
	return 0;
}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement)
{
	NIDAQmxManager_type*			NIDAQ	 				= mod;
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	//--------------------------------------------------------------------------
	// Save DAQmanager main panel position
	//--------------------------------------------------------------------------
	
	int								panTopPos				= 0;
	int								panLeftPos				= 0;
	DAQLabXMLNode 					NIDAQAttr[] 			= {	{"PanTopPos", BasicData_Int, &panTopPos},
											  		   			{"PanLeftPos", BasicData_Int, &panLeftPos}	};
	
	errChk( GetPanelAttribute(NIDAQ->mainPanHndl, ATTR_LEFT, &panLeftPos) );
	errChk( GetPanelAttribute(NIDAQ->mainPanHndl, ATTR_TOP, &panTopPos) );
	DLAddToXMLElem(xmlDOM, moduleElement, NIDAQAttr, DL_ATTRIBUTE, NumElem(NIDAQAttr)); 
	
	//--------------------------------------------------------------------------
	// Save NIDAQ device configuration
	//--------------------------------------------------------------------------
	
	size_t							nDevices				= ListNumItems(NIDAQ->DAQmxDevices);
	Dev_type*						dev						= NULL;
	ActiveXMLObj_IXMLDOMElement_	NIDAQDeviceXMLElement	= 0;	 // holds an "NIDAQDevice" element to which device settings are added
	
	for (size_t i = 1; i <= nDevices; i++) {
		dev = *(Dev_type**)ListGetPtrToItem(NIDAQ->DAQmxDevices, i);
		// create new device XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "NIDAQDevice", &NIDAQDeviceXMLElement) );
		// save device config
		errChk( SaveDeviceCfg(dev, xmlDOM, NIDAQDeviceXMLElement) );
		// add device XML element to NIDAQ module
		errChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, &xmlERRINFO, NIDAQDeviceXMLElement, NULL) );
		// discard device XML element handle
		OKFreeCAHandle(NIDAQDeviceXMLElement); 
	}
	
	return 0;
	
Error:
	
	return error;
}

static int SaveDeviceCfg (Dev_type* dev, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ NIDAQDeviceXMLElement) 
{
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	//--------------------------------------------------------------------------------
	// Save  	- device product ID that must match when the settings are loaded again
	//			- task controller name
	//--------------------------------------------------------------------------------
	
	char*							tcName					= GetTaskControlName(dev->taskController);
	DAQLabXMLNode 					DevAttr[] 				= {	{"NIProductClassID", BasicData_UInt, &dev->attr->productNum},
																{"Name", BasicData_CString, tcName}	};
	
	DLAddToXMLElem(xmlDOM, NIDAQDeviceXMLElement, DevAttr, DL_ATTRIBUTE, NumElem(DevAttr)); 
	OKfree(tcName);
	
	//--------------------------------------------------------------------------------
	// Save DAQmx tasks
	//--------------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMElement_	AITaskXMLElement	= 0;	 // holds an "AITask" element
	ActiveXMLObj_IXMLDOMElement_	AOTaskXMLElement	= 0;	 // holds an "AOTask" element
	ActiveXMLObj_IXMLDOMElement_	DITaskXMLElement	= 0;	 // holds an "DITask" element
	ActiveXMLObj_IXMLDOMElement_	DOTaskXMLElement	= 0;	 // holds an "DOTask" element
	ActiveXMLObj_IXMLDOMElement_	CITaskXMLElement	= 0;	 // holds an "CITask" element
	ActiveXMLObj_IXMLDOMElement_	COTaskXMLElement	= 0;	 // holds an "COTask" element
	
	// AI task
	if (dev->AITaskSet) {
		// create new task XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "AITask", &AITaskXMLElement) );
		// save task config
		errChk( SaveAITaskCfg (dev->AITaskSet, xmlDOM, AITaskXMLElement) );
		// add task XML element to device element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (NIDAQDeviceXMLElement, &xmlERRINFO, AITaskXMLElement, NULL) );
		// discard task XML element handle
		OKFreeCAHandle(AITaskXMLElement);
	}
	
	// AO task
	if (dev->AOTaskSet) {
		// create new task XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "AOTask", &AOTaskXMLElement) );
		// save task config
		errChk( SaveAOTaskCfg (dev->AOTaskSet, xmlDOM, AOTaskXMLElement) );
		// add device XML element to device element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (NIDAQDeviceXMLElement, &xmlERRINFO, AOTaskXMLElement, NULL) );
		// discard task XML element handle
		OKFreeCAHandle(AOTaskXMLElement);
	}
	
	return 0;
	
Error:
	
	return error;
}
		
static int SaveAITaskCfg (ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement) 
{
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	//--------------------------------------------------------------------------------
	// Save task properties
	//--------------------------------------------------------------------------------
	
	uInt32							operationMode			= (uInt32)task->timing->measMode;
	uInt32							sampleClockEdge			= (uInt32)task->timing->sampClkEdge;
	
	DAQLabXMLNode 					taskAttr[] 				= {	{"Timeout", BasicData_Double, &task->timeout},
																{"OperationMode", BasicData_UInt, &operationMode},
																{"SamplingRate", BasicData_Double, &task->timing->sampleRate},
																{"NSamples", BasicData_ULongLong, &task->timing->nSamples},
																{"BlockSize", BasicData_UInt, &task->timing->blockSize},
																{"SampleClockSource", BasicData_CString, task->timing->sampClkSource},
																{"SampleClockEdge", BasicData_UInt, &sampleClockEdge},
																{"ReferenceClockSource", BasicData_CString, task->timing->refClkSource},
																{"ReferenceClockFrequency", BasicData_Double, &task->timing->refClkFreq} };
																	
	DLAddToXMLElem(xmlDOM, AITaskXMLElement, taskAttr, DL_ATTRIBUTE, NumElem(taskAttr)); 
	
	//--------------------------------------------------------------------------------
	// Save task triggers
	//--------------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMElement_	TriggersXMLElement			= 0;	 // holds the "Triggers" element
	ActiveXMLObj_IXMLDOMElement_	StartTriggerXMLElement		= 0;	 // holds the "StartTrigger" element
	ActiveXMLObj_IXMLDOMElement_	ReferenceTriggerXMLElement	= 0;	 // holds the "ReferenceTrigger" element
	
	// create new triggers XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "Triggers", &TriggersXMLElement) );
	
	//----------------------
	// Start Trigger
	//----------------------
	if (task->startTrig) {
		// create "StartTrigger" XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "StartTrigger", &StartTriggerXMLElement) );
		// save trigger info
		errChk( SaveTaskTrigCfg(task->startTrig, xmlDOM, StartTriggerXMLElement) ); 
		// add "StartTrigger" XML element to "Triggers" XML element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (TriggersXMLElement, &xmlERRINFO, StartTriggerXMLElement, NULL) );
		// discard "StartTrigger" XML element handle
		OKFreeCAHandle(StartTriggerXMLElement);
	}
	
	//----------------------
	// Reference Trigger
	//----------------------
	if (task->referenceTrig) {
		// create "ReferenceTrigger" XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "ReferenceTrigger", &ReferenceTriggerXMLElement) );
		// save trigger info
		errChk( SaveTaskTrigCfg(task->referenceTrig, xmlDOM, ReferenceTriggerXMLElement) );
		DAQLabXMLNode 	refTrigAttr[] = {{"NPreTrigSamples", BasicData_UInt, &task->referenceTrig->nPreTrigSamples}};
		errChk( DLAddToXMLElem(xmlDOM, ReferenceTriggerXMLElement, refTrigAttr, DL_ATTRIBUTE, NumElem(refTrigAttr)) ); 
		// add "ReferenceTrigger" XML element to "Triggers" XML element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (TriggersXMLElement, &xmlERRINFO, ReferenceTriggerXMLElement, NULL) );
		// discard "ReferenceTrigger" XML element handle
		OKFreeCAHandle(ReferenceTriggerXMLElement);
	}
	
	// add "Triggers" XML element to task element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (AITaskXMLElement, &xmlERRINFO, TriggersXMLElement, NULL) );
	// discard "Triggers" XML element handle
	OKFreeCAHandle(TriggersXMLElement);
	
	//--------------------------------------------------------------------------------
	// Save task channels
	//--------------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMElement_	ChannelsXMLElement			= 0;	 // holds the "Channels" element
	ActiveXMLObj_IXMLDOMElement_	ChannelXMLElement			= 0;	 // holds the "Channel" element for each channel in the task
	size_t							nChannels					= ListNumItems(task->chanSet);
	ChanSet_type*					chanSet						= NULL;
	
	// create new "Channels" XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "Channels", &ChannelsXMLElement) );
	// save channels info
	for (size_t i = 1; i <= nChannels; i++) {
		chanSet = *(ChanSet_type**) ListGetPtrToItem(task->chanSet, i);
		// create new "Channel" element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, &xmlERRINFO, "Channel", &ChannelXMLElement) );
		// save channel info
		errChk( SaveChannelCfg(chanSet, xmlDOM, ChannelXMLElement) );
		// add "Channel" element to "Channels" element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (ChannelsXMLElement, &xmlERRINFO, ChannelXMLElement, NULL) );
		// discard "Channel" XML element handle
		OKFreeCAHandle(ChannelXMLElement);
	}
	
	// add "Channels" XML element to task element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (AITaskXMLElement, &xmlERRINFO, ChannelsXMLElement, NULL) );
	// discard "Channels" XML element handle
	OKFreeCAHandle(ChannelsXMLElement);
	
	return 0;
	
Error:
	
	return error;	
}

static int SaveAOTaskCfg (ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement) 
{
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	
	return 0;
	
Error:
	
	return error;	
}

static int SaveDITaskCfg (ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement) 
{
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	
	return 0;
	
Error:
	
	return error;	
}

static int SaveDOTaskCfg (ADTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement) 
{
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	
	return 0;
	
Error:
	
	return error;	
}

static int SaveCITaskCfg (CITaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement) 
{
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	
	return 0;
	
Error:
	
	return error;	
}

static int SaveCOTaskCfg (COTaskSet_type* task, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement) 
{
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	
	return 0;
	
Error:
	
	return error;	
}

static int SaveTaskTrigCfg (TaskTrig_type* taskTrig, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ TriggerXMLElement)
{
#define SaveTaskTrigCfg_Err_NotImplemented		-1
	
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	uInt32							trigType				= (uInt32) taskTrig->trigType;
	uInt32							slopeType				= (uInt32) taskTrig->slope;
	uInt32							triggerCondition		= (uInt32) taskTrig->wndTrigCond;
		// shared trigger attributes
	DAQLabXMLNode 					sharedTrigAttr[]		= {	{"Type", BasicData_UInt, &trigType},
																{"Source", BasicData_CString, taskTrig->trigSource} };
		// for both analog and digital edge triggers
	DAQLabXMLNode 					edgeTrigAttr[]			= {	{"Slope", BasicData_UInt, &slopeType} };
		// for analog edge trigger
	DAQLabXMLNode 					levelTrigAttr[]			= {	{"Level", BasicData_Double, &taskTrig->level} };
		// for analog window trigger
	DAQLabXMLNode 					windowTrigAttr[]		= {	{"WindowTop", BasicData_Double, &taskTrig->wndTop},
																{"WindowBottom", BasicData_Double, &taskTrig->wndBttm},
																{"WindowTriggerCondition", BasicData_UInt, &triggerCondition} };

	// add shared trigger attributes 
	DLAddToXMLElem(xmlDOM, TriggerXMLElement, sharedTrigAttr, DL_ATTRIBUTE, NumElem(sharedTrigAttr)); 
	
	// add specific trigger attributes
	switch (taskTrig->trigType) {
			
		case Trig_None:
			
			break;
			
		case Trig_DigitalEdge:
			
			DLAddToXMLElem(xmlDOM, TriggerXMLElement, edgeTrigAttr, DL_ATTRIBUTE, NumElem(edgeTrigAttr)); 
			
			break;
			
		case Trig_DigitalPattern:
			
			return SaveTaskTrigCfg_Err_NotImplemented;
			
		case Trig_AnalogEdge:
			
			DLAddToXMLElem(xmlDOM, TriggerXMLElement, edgeTrigAttr, DL_ATTRIBUTE, NumElem(edgeTrigAttr)); 
			DLAddToXMLElem(xmlDOM, TriggerXMLElement, levelTrigAttr, DL_ATTRIBUTE, NumElem(levelTrigAttr)); 
			
			break;
			
		case Trig_AnalogWindow:
			
			DLAddToXMLElem(xmlDOM, TriggerXMLElement, windowTrigAttr, DL_ATTRIBUTE, NumElem(windowTrigAttr)); 
	}
									
	
	return 0;
	
Error:
	
	return error;
}

static int SaveChannelCfg (ChanSet_type* chanSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ ChannelXMLElement)
{
#define SaveChannelCfg_Err_NotImplemented		-1
	
	int								error					= 0;
	ERRORINFO						xmlERRINFO;
	
	
	
	return 0;
	
Error:
	
	return error;
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

//------------------------------------------------------------------------------
// Dev_type
//------------------------------------------------------------------------------

static Dev_type* init_Dev_type (NIDAQmxManager_type* niDAQModule, DevAttr_type* attr, char taskControllerName[])
{
	Dev_type* dev = malloc (sizeof(Dev_type));
	if (!dev) return NULL;
	
	// init
	dev -> nActiveTasks			= 0;
	dev -> taskController		= NULL;
	
	//-------------------------------------------------------------------------------------------------
	// Task Controller
	//-------------------------------------------------------------------------------------------------
	dev -> taskController = init_TaskControl_type (taskControllerName, dev, DLGetCommonThreadPoolHndl(), ConfigureTC, UnconfigureTC, IterateTC, AbortIterationTC, StartTC, 
						  ResetTC, DoneTC, StoppedTC, TaskTreeStatus, NULL, ModuleEventHandler, ErrorTC);
	if (!dev->taskController) goto Error;
	
	dev -> niDAQModule			= niDAQModule;
	dev -> attr					= attr;  
	dev -> devPanHndl			= 0;
	
	//--------------------------
	// DAQmx task settings
	//--------------------------
	
	dev -> AITaskSet			= NULL;			
	dev -> AOTaskSet			= NULL;
	dev -> DITaskSet			= NULL;
	dev -> DOTaskSet			= NULL;
	dev -> CITaskSet			= NULL;
	dev -> COTaskSet			= NULL;	
	
	//----------------------------------------
	// Active DAQmx tasks thread safe variable
	//----------------------------------------
	
	if (CmtNewTSV(sizeof(int), &dev->nActiveTasks) < 0) goto Error;
	
	
	return dev;
	
Error:
	
	discard_TaskControl_type(&dev->taskController);
	OKfree(dev);
	return NULL;
}

static void discard_Dev_type(Dev_type** devPtr)
{
	if (!*devPtr) return;
	
	// device properties
	discard_DevAttr_type(&(*devPtr)->attr);
	
	// task controller for this device
	discard_TaskControl_type(&(*devPtr)->taskController);
	
	// active tasks thread safe variable
	if ((*devPtr)->nActiveTasks)
		CmtDiscardTSV((*devPtr)->nActiveTasks);
	
	// DAQmx task settings
	discard_ADTaskSet_type(&(*devPtr)->AITaskSet);
	discard_ADTaskSet_type(&(*devPtr)->AOTaskSet);
	discard_ADTaskSet_type(&(*devPtr)->DITaskSet);
	discard_ADTaskSet_type(&(*devPtr)->DOTaskSet);
	discard_CITaskSet_type(&(*devPtr)->CITaskSet);
	discard_COTaskSet_type(&(*devPtr)->COTaskSet);
	
	OKfree(*devPtr);
}

//------------------------------------------------------------------------------
// Detection of connected DAQmx devices
//------------------------------------------------------------------------------

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
		
		// Retrieving these attributes for simulated devices generates error (property not suported)
		// This is quite strange cause this misses the point of a simulated device
		
		// 16. AO max rate  
//		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MaxRate, &devAttrPtr->AOmaxrate)); 			 	// [Hz]
		// 17. AO min rate  
//		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MinRate, &devAttrPtr->AOminrate)); 			 	// [Hz]
		// 18. DI max rate
//		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_MaxRate, &devAttrPtr->DImaxrate)); 			 	// [Hz]
		// 19. DO max rate
//		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_MaxRate, &devAttrPtr->DOmaxrate));    		 		// [Hz]
		
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

//------------------------------------------------------------------------------
// DevAttr_type Device attributes data structure
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
	if ((*a)->AIchan) { 	
		ListApplyToEachEx ((*a)->AIchan, 1, DisposeAIChannelList, NULL); 
		ListDispose((*a)->AIchan); 
	}
	
	if ((*a)->AOchan) {
		ListApplyToEachEx ((*a)->AOchan, 1, DisposeAOChannelList, NULL); 
		ListDispose((*a)->AOchan);	  
	}
	
	if ((*a)->DIlines) {
		ListApplyToEachEx ((*a)->DIlines, 1, DisposeDILineChannelList, NULL); 
		ListDispose((*a)->DIlines);  
	}
	
	if ((*a)->DIports) {
		ListApplyToEachEx ((*a)->DIports, 1, DisposeDIPortChannelList, NULL); 
		ListDispose((*a)->DIports);  
	}
	
	if ((*a)->DOlines) {
		ListApplyToEachEx ((*a)->DOlines, 1, DisposeDOLineChannelList, NULL); 
		ListDispose((*a)->DOlines);
	}
	
	if ((*a)->DOports) {
		ListApplyToEachEx ((*a)->DOports, 1, DisposeDOPortChannelList, NULL); 
		ListDispose((*a)->DOports);  
	}
	
	if ((*a)->CIchan) {
		ListApplyToEachEx ((*a)->CIchan, 1, DisposeCIChannelList, NULL); 
		ListDispose((*a)->CIchan);
	}
	
	if ((*a)->COchan) {
		ListApplyToEachEx ((*a)->COchan, 1, DisposeCOChannelList, NULL); 
		ListDispose((*a)->COchan);
	}
	
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
	size_t				nChans		= ListNumItems(dev->attr->AIchan); 	
	for (size_t i = 1; i <= nChans; i++) { 
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
	size_t				nChans		= ListNumItems(dev->attr->AOchan); 	
	for (size_t i = 1; i <= nChans; i++) {
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
	size_t					nChans		= ListNumItems(dev->attr->DIlines); 	
	for (size_t i = 1; i <= nChans; i++) {
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
	size_t					nChans		= ListNumItems(dev->attr->DIports); 	
	for (size_t i = 1; i <= nChans; i++) {
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
	size_t					nChans		= ListNumItems(dev->attr->DOlines); 	
	for (size_t i = 1; i <= nChans; i++) {	
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
	size_t					nChans		= ListNumItems(dev->attr->DOports); 	
	for (size_t i = 1; i <= nChans; i++) {
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
	size_t				nChans		= ListNumItems(dev->attr->CIchan); 	
	for (size_t i = 1; i <= nChans; i++) {	
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
	size_t				nChans		= ListNumItems(dev->attr->COchan); 	
	for (size_t i = 1; i <= nChans; i++) {		
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
// ChanSet_type base class
//------------------------------------------------------------------------------

static void  init_ChanSet_type (Dev_type* dev, ChanSet_type* chanSet, char physChanName[], Channel_type chanType, DiscardChanSetFptr_type discardFptr)
{
	chanSet -> name				= StrDup(physChanName);
	chanSet -> chanType			= chanType;
	chanSet -> chanPanHndl		= 0;
	chanSet -> sinkVChan		= NULL;
	chanSet -> device			= dev;
	chanSet -> srcVChan			= NULL;
	chanSet -> onDemand			= FALSE;	// hw-timing by default
	chanSet -> discardFptr		= discardFptr;
	chanSet -> startTrig		= NULL;
	chanSet -> HWTrigMaster		= NULL;
	chanSet -> HWTrigSlave		= NULL;
	chanSet -> referenceTrig	= NULL;
	chanSet -> taskHndl			= NULL;
	chanSet -> ScaleMax			= 0;
	chanSet -> ScaleMin			= 0;
	chanSet -> offset			= 0;
	chanSet -> gain				= 1;
	chanSet -> integrateFlag	= FALSE;
}

static void	discard_ChanSet_type (ChanSet_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->name);
	
	// VChans
	discard_VChan_type((VChan_type**)&(*a)->srcVChan);
	discard_VChan_type((VChan_type**)&(*a)->sinkVChan);
	
	// discard DAQmx task if any
	if ((*a)->taskHndl) {
		DAQmxTaskControl((*a)->taskHndl, DAQmx_Val_Task_Abort);
		DAQmxClearTask ((*a)->taskHndl); 
		(*a)->taskHndl = 0;
	}
	
	// discard triggers if any
	discard_TaskTrig_type(&(*a)->startTrig);
	discard_TaskTrig_type(&(*a)->referenceTrig);
	
	// discard HW triggers
	discard_HWTrigMaster_type(&(*a)->HWTrigMaster);
	discard_HWTrigSlave_type(&(*a)->HWTrigSlave);
	
	
	OKfree(*a);
}

//------------------------------------------------------------------------------
// ChanSet_AIAO_Voltage_type
//------------------------------------------------------------------------------
static ChanSet_type* init_ChanSet_AIAO_Voltage_type (Dev_type* dev, char physChanName[], Channel_type chanType, AIChannel_type* aiChanAttrPtr, AOChannel_type* aoChanAttrPtr)
{
	ChanSet_AIAO_Voltage_type* a = malloc (sizeof(ChanSet_AIAO_Voltage_type));
	if (!a) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &a->baseClass, physChanName, chanType, discard_ChanSet_AIAO_Voltage_type);
	
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
	ChanSet_AIAO_Voltage_type* 	chSetPtr 		= callbackData;
	Dev_type*					dev				= chSetPtr->baseClass.device;
	char*						errMsg			= NULL;
	int							error			= 0;

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
			
			// update device settings
			errChk( ConfigDAQmxAITask(chSetPtr->baseClass.device, &errMsg) );
			errChk ( ConfigDAQmxAOTask(chSetPtr->baseClass.device, &errMsg) );
			
			break;
			
		case AIAOChSet_Range:
			
			int rangeIdx;
			GetCtrlVal(panel, control, &rangeIdx);
			
			if (chSetPtr->aiChanAttr) {
				
				// read in ranges
				chSetPtr->Vmax = chSetPtr->aiChanAttr->Vrngs->high[rangeIdx];
				chSetPtr->Vmin = chSetPtr->aiChanAttr->Vrngs->low[rangeIdx];
				//---------------------------
				// adjust data type scaling
				//---------------------------
				// max value
				chSetPtr->baseClass.ScaleMax = chSetPtr->Vmax; 
				SetCtrlVal(chSetPtr->baseClass.chanPanHndl, AIAOChSet_ScaleMax, chSetPtr->baseClass.ScaleMax); 
				SetCtrlAttribute(chSetPtr->baseClass.chanPanHndl, AIAOChSet_ScaleMax, ATTR_MAX_VALUE, chSetPtr->Vmax);
				SetCtrlAttribute(chSetPtr->baseClass.chanPanHndl, AIAOChSet_ScaleMax, ATTR_MIN_VALUE, chSetPtr->Vmin);
				// min value
				chSetPtr->baseClass.ScaleMin = chSetPtr->Vmin;
				SetCtrlVal(chSetPtr->baseClass.chanPanHndl, AIAOChSet_ScaleMin, chSetPtr->baseClass.ScaleMin); 
				SetCtrlAttribute(chSetPtr->baseClass.chanPanHndl, AIAOChSet_ScaleMin, ATTR_MAX_VALUE, chSetPtr->Vmax);
				SetCtrlAttribute(chSetPtr->baseClass.chanPanHndl, AIAOChSet_ScaleMin, ATTR_MIN_VALUE, chSetPtr->Vmin);
							
			} else {
				chSetPtr->Vmax = chSetPtr->aoChanAttr->Vrngs->high[rangeIdx];
				chSetPtr->Vmin = chSetPtr->aoChanAttr->Vrngs->low[rangeIdx];
			}
			
			// update device settings
			errChk ( ConfigDAQmxAITask(chSetPtr->baseClass.device, &errMsg) );
			errChk ( ConfigDAQmxAOTask(chSetPtr->baseClass.device, &errMsg) );
			
			break;
			
		case AIAOChSet_AIDataType:
			
			int AIDataTypeIdx;
			
			GetCtrlIndex(panel, control, &AIDataTypeIdx);
			BOOL showScaleControls = FALSE;
			switch (AIDataTypeIdx) {
					
				case AI_Double:
					SetSourceVChanDataType(chSetPtr->baseClass.srcVChan, DL_Waveform_Double);
					break;
					
				case AI_Float:
					SetSourceVChanDataType(chSetPtr->baseClass.srcVChan, DL_Waveform_Float);
					break;
					
				case AI_UInt:
					SetSourceVChanDataType(chSetPtr->baseClass.srcVChan, DL_Waveform_UInt);
					showScaleControls = TRUE;
					break;
					
				case AI_UShort:
					SetSourceVChanDataType(chSetPtr->baseClass.srcVChan, DL_Waveform_UShort);
					showScaleControls = TRUE;
					break;
					
				case AI_UChar:
					SetSourceVChanDataType(chSetPtr->baseClass.srcVChan, DL_Waveform_UChar);
					showScaleControls = TRUE;
					break;
			}
			
			// dim/undim scale controls
			SetCtrlAttribute(panel, AIAOChSet_ScaleMin, ATTR_VISIBLE, showScaleControls);
			SetCtrlAttribute(panel, AIAOChSet_ScaleMax, ATTR_VISIBLE, showScaleControls);
			
			DLUpdateSwitchboard();
			
			break;
			
		case AIAOChSet_Offset:
			
			GetCtrlVal(panel, control, &chSetPtr->baseClass.offset);
			
			break;
			
		case AIAOChSet_Gain:
			
			GetCtrlVal(panel, control, &chSetPtr->baseClass.gain);
			
			break;
			
		case AIAOChSet_Integrate:
			
			GetCtrlVal(panel, control, &chSetPtr->baseClass.integrateFlag);
			
			break;
			
		case AIAOChSet_ScaleMin:
			
			GetCtrlVal(panel, control, &chSetPtr->baseClass.ScaleMin);
			
			break;
			
		case AIAOChSet_ScaleMax:
			
			GetCtrlVal(panel, control, &chSetPtr->baseClass.ScaleMax);
			
			break;
				
		case AIAOChSet_Terminal:
			
			int terminalType;
			GetCtrlVal(panel, control, &terminalType);
			chSetPtr->terminal = (Terminal_type) terminalType;
			
			// update device settings
			errChk ( ConfigDAQmxAITask(chSetPtr->baseClass.device, &errMsg) );
			errChk ( ConfigDAQmxAOTask(chSetPtr->baseClass.device, &errMsg) );
			
			break;
			
		case AIAOChSet_VChanName:
			
			char*	newName = NULL;
			newName = GetStringFromControl (panel, control);
			if (chSetPtr->aiChanAttr) 
				// analong input channel type
				DLRenameVChan((VChan_type*)chSetPtr->baseClass.srcVChan, newName);
			else
				// analog output channel type
				DLRenameVChan((VChan_type*)chSetPtr->baseClass.sinkVChan, newName);
			
			OKfree(newName);
			
			break;
			
			
	}
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

//------------------------------------------------------------------------------
// ChanSet_DIDO_type
//------------------------------------------------------------------------------
static ChanSet_DIDO_type* init_ChanSet_DIDO_type (Dev_type* dev, char physChanName[], Channel_type chanType) 
{
	ChanSet_DIDO_type* a = malloc (sizeof(ChanSet_DIDO_type));
	if (!a) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &a->baseClass, physChanName, chanType, discard_ChanSet_DIDO_type);
	
	// init child
	a -> invert		= FALSE;
	
	return a;
}

static void discard_ChanSet_DIDO_type (ChanSet_type** a)
{
	if (!*a) return; 
	
	// discard base class
	discard_ChanSet_type(a);	
}

// DO channel UI callback for setting properties
static int ChanSetLineDO_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_DIDO_type*  	chanset			= callbackData; 
	char*					errMsg			= NULL; 
	int						error			= 0;
	uInt32 					lineVal			= 0;  // line value

	if (event != EVENT_COMMIT) return 0;
	
	switch(control) {
		case DIDOLChSet_OnDemand:
			
			GetCtrlVal(panel, control, &chanset->baseClass.onDemand);
			// if this is an On Demand DO channel , adjust VChan such that only Waveform_type* data is allowed
			// otherwise allow both RepeatedWaveform_type* and Waveform_type* data
			if (chanset->baseClass.onDemand) {
				
				DLDataTypes allowedTypes[] = {	
						DL_Waveform_Char,
						DL_Waveform_UChar,
						DL_Waveform_Short,
						DL_Waveform_UShort,
						DL_Waveform_Int,
						DL_Waveform_UInt, };
						
				SetSinkVChanDataTypes(chanset->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes);
				
			} else {
				
				DLDataTypes allowedTypes[] = {	
						DL_Waveform_Char,
						DL_Waveform_UChar,
						DL_Waveform_Short,
						DL_Waveform_UShort,
						DL_Waveform_Int,
						DL_Waveform_UInt,
						DL_RepeatedWaveform_Char,						
						DL_RepeatedWaveform_UChar,						
						DL_RepeatedWaveform_Short,						
						DL_RepeatedWaveform_UShort,					
						DL_RepeatedWaveform_Int,						
						DL_RepeatedWaveform_UInt};
						
				SetSinkVChanDataTypes(chanset->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes);
			}
			
			// configure DO task
			errChk (ConfigDAQmxDOTask(chanset->baseClass.device, &errMsg) );   
			
			break;
		
		case DIDOLChSet_VChanName:
			
			char*	newName = NULL;
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)chanset->baseClass.sinkVChan, newName); 
			OKfree(newName); 
			break;
			
		case DIDOLChSet_Invert:
			
			GetCtrlVal(panel,DIDOLChSet_Invert,&chanset->invert);
			
			// configure DO task
			errChk(ConfigDAQmxDOTask(chanset->baseClass.device, &errMsg) );
			
			break;
			
		case DIDOLChSet_OutputBTTN:
			
			break;
			
	}
	
    GetCtrlVal(panel, DIDOLChSet_OutputBTTN, &lineVal);       
	
	// update output
 	DirectDigitalOut(chanset->baseClass.name, lineVal, chanset->invert, DAQmxDefault_Task_Timeout);
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

// DO channel UI callback for setting properties
static int ChanSetPortDO_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_DIDO_type*  chanset 	= callbackData; 
	char*				errMsg		= NULL; 
	int					error		= 0;
	uInt32 				data 		= 0;     	// port value
	uInt32 				lineval		= 0;  		// line value
	
	if (event != EVENT_COMMIT) return 0;		// do nothing
	
	switch(control) {
			
		case DIDOPChSet_VChanName:
			
			char*	newName = NULL;
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)chanset->baseClass.sinkVChan, newName); 
			OKfree(newName);    
			break;
			
		case DIDOPChSet_Invert:
			
			GetCtrlVal(panel,DIDOPChSet_Invert, &chanset->invert);  
			
			// configure DO task
			errChk(ConfigDAQmxDOTask(chanset->baseClass.device, &errMsg) );
			
			break;
			
		case DIDOPChSet_RADIOBUTTON_0:
		case DIDOPChSet_RADIOBUTTON_1: 	
		case DIDOPChSet_RADIOBUTTON_2: 	
		case DIDOPChSet_RADIOBUTTON_3: 	
		case DIDOPChSet_RADIOBUTTON_4:
		case DIDOPChSet_RADIOBUTTON_5: 
		case DIDOPChSet_RADIOBUTTON_6: 
		case DIDOPChSet_RADIOBUTTON_7:
			
			break;
	
	}
	
	//get all radiobutton values
	
	data=0;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_7,&lineval);
	data+=lineval;
	data<<=1;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_6,&lineval);
	data+=lineval;
	data<<=1;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_5,&lineval);
	data+=lineval;
	data<<=1;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_4,&lineval);
	data+=lineval;
	data<<=1;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_3,&lineval);
	data+=lineval;
	data<<=1;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_2,&lineval);
	data+=lineval;
	data<<=1;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_1,&lineval);
	data+=lineval;
	data<<=1;
	GetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_0,&lineval);
	data+=lineval;
		
	 // update output
 	DirectDigitalOut(chanset->baseClass.name, data, chanset->invert, DAQmxDefault_Task_Timeout);
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

int ReadDirectDigitalOut(char* chan,uInt32* data)
{
	int 		error=0;
	TaskHandle  taskHandle=0;
	int32		written;

	DAQmxErrChk (DAQmxCreateTask("",&taskHandle));
	DAQmxErrChk (DAQmxCreateDOChan(taskHandle,chan,"",DAQmx_Val_ChanForAllLines));
	DAQmxErrChk (DAQmxStartTask(taskHandle));
	DAQmxErrChk (DAQmxReadDigitalU32(taskHandle,1,10.0,DAQmx_Val_GroupByChannel,data,1,&written,NULL));
	
DAQmxError:
	if( DAQmxFailed(error) )
		error=-1;
	
	if( taskHandle!=0 ) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandle);
		DAQmxClearTask(taskHandle);
	}
	
	
	return error;
}

int DirectDigitalOut (char* chan,uInt32 data,BOOL invert, double timeout)
{
	int 		error=0;
	TaskHandle  taskHandle=0;
	int32		written;

	if (invert) data=~data;   //invert data
	DAQmxErrChk (DAQmxCreateTask("", &taskHandle));
	DAQmxErrChk (DAQmxCreateDOChan(taskHandle, chan, "", DAQmx_Val_ChanForAllLines));
	DAQmxErrChk (DAQmxStartTask(taskHandle));
	DAQmxErrChk (DAQmxWriteDigitalU32(taskHandle,1,1,timeout,DAQmx_Val_GroupByChannel,&data,&written,NULL));
	
DAQmxError:
	
	if( DAQmxFailed(error) )
		error=-1;
	
	if( taskHandle!=0 ) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandle);
		DAQmxClearTask(taskHandle);
	}
	
	return error;
}

void SetPortControls(int panel,uInt32 data)
{
	//calculate all radiobutton values 
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_0,data&0x01);
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_1,data&0x02);
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_2,data&0x04);
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_3,data&0x08);
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_4,data&0x10);
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_5,data&0x20);
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_6,data&0x40);
	SetCtrlVal(panel,DIDOPChSet_RADIOBUTTON_7,data&0x80);
}

//------------------------------------------------------------------------------
// ChanSet_CI_Frequency_type
//------------------------------------------------------------------------------
static ChanSet_type* init_ChanSet_CI_Frequency_type (Dev_type* dev, char physChanName[])
{
	ChanSet_CI_Frequency_type* a = malloc (sizeof(ChanSet_CI_Frequency_type));
	if (!a) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &a->baseClass, physChanName, Chan_CI_Frequency, discard_ChanSet_CI_Frequency_type); 
	
			a -> divisor 			= DAQmxDefault_CI_Frequency_Task_Divisor;
			a -> edgeType			= DAQmxDefault_CI_Frequency_Task_Edge;
			a -> freqMax			= DAQmxDefault_CI_Frequency_Task_MaxFreq;
			a -> freqMin			= DAQmxDefault_CI_Frequency_Task_MinFreq;
			a -> measMethod			= DAQmxDefault_CI_Frequency_Task_MeasMethod;
			a -> measTime			= DAQmxDefault_CI_Frequency_Task_MeasTime;
			a -> freqInputTerminal  = NULL;
	if (!(  a -> taskTiming			= init_CounterTaskTiming_type()))	goto Error;
	
	
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
	discard_CounterTaskTiming_type((TaskTiming_type**)&chCIFreqPtr->taskTiming);
	
	// discard base class
	discard_ChanSet_type(a);
}

// CI Edge channel UI callback for setting channel properties
static int ChanSetCIEdge_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	CIEdgeChanSet_type* 	chSetPtr 	= callbackData;
	char*					newName 	= NULL; 
	if (event != EVENT_COMMIT) return 0;
	
	switch(control) {
			
		case SETPAN_SrcVChanName:
			
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)chSetPtr->baseClass.srcVChan, newName);
			OKfree(newName);
			
			break;
			
		case SETPAN_SinkVChanName:
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)chSetPtr->baseClass.sinkVChan, newName);
			OKfree(newName);
			
			break;
	}
	
	return 0;
}

//------------------------------------------------------------------------------
// ChanSet_CO_type
//------------------------------------------------------------------------------
static ChanSet_type* init_ChanSet_CO_type (Dev_type* dev, char physChanName[], PulseTrainTimingTypes pulseTrainType)
{
	Channel_type 			chanType	= Chan_CO_Pulse_Frequency;
	ChanSet_CO_type* 		COChanSet	= malloc (sizeof(ChanSet_CO_type));
	
	if (!COChanSet) return NULL;
	
	// comvert channel type to pulse train type
	switch (pulseTrainType) {
			
		case PulseTrain_Freq:
			chanType 				= Chan_CO_Pulse_Frequency;
			COChanSet->pulseTrain   = (PulseTrain_type*) init_PulseTrainFreqTiming_type(PulseTrain_Finite, DAQmxDefault_CO_Task_idlestate, DAQmxDefault_CO_Task_nPulses, 
									  DAQmxDefault_CO_Frequency_Task_dutycycle, DAQmxDefault_CO_Frequency_Task_freq, DAQmxDefault_CO_Frequency_Task_initdelay); 
			break;
			
		case PulseTrain_Time:
			chanType = Chan_CO_Pulse_Time;
			COChanSet->pulseTrain   = (PulseTrain_type*) init_PulseTrainTimeTiming_type(PulseTrain_Finite, DAQmxDefault_CO_Task_idlestate, DAQmxDefault_CO_Task_nPulses, 
									  DAQmxDefault_CO_Time_Task_hightime, DAQmxDefault_CO_Time_Task_lowtime, DAQmxDefault_CO_Time_Task_initdelay); 
			break;
			
		case PulseTrain_Ticks:
			chanType = Chan_CO_Pulse_Ticks;
			COChanSet->pulseTrain   = (PulseTrain_type*) init_PulseTrainTickTiming_type(PulseTrain_Finite, DAQmxDefault_CO_Task_idlestate, DAQmxDefault_CO_Task_nPulses,
									  DAQmxDefault_CO_Ticks_Task_highticks, DAQmxDefault_CO_Ticks_Task_lowticks, DAQmxDefault_CO_Ticks_Task_initdelayticks);  
			break;
	}
	
	// initialize base class
	init_ChanSet_type(dev, &COChanSet->baseClass, physChanName, chanType, discard_ChanSet_CO_type); 
	
	if (!(  COChanSet -> taskTiming	= init_CounterTaskTiming_type()))	goto Error;      
	
	return (ChanSet_type*)COChanSet;
Error:
	OKfree(COChanSet);
	return NULL;
}

static void discard_ChanSet_CO_type (ChanSet_type** a)
{
	if (!*a) return; 
	
	ChanSet_CO_type* chCOPtr = *(ChanSet_CO_type**) a;
	
	discard_PulseTrain_type (&chCOPtr->pulseTrain);
	
	discard_CounterTaskTiming_type((TaskTiming_type**)&chCOPtr->taskTiming);
	
	// discard base class
	discard_ChanSet_type(a);
}

// CO channel UI callback for setting properties
static int ChanSetCO_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type*	COChan 			= callbackData;
	char*				newName 		= NULL;
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;	// do nothing
	
	switch(control) {
			
		case SETPAN_Timeout:
			
			GetCtrlVal(panel, control, &COChan->baseClass.device->COTaskSet->timeout);
			
			// configure CO task
			errChk ( ConfigDAQmxCOTask(COChan->baseClass.device, &errMsg) ); 
			
			break;
		
		case SETPAN_SrcVChanName:
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)COChan->baseClass.srcVChan, newName);
			OKfree(newName);
			
			break;
			
		case SETPAN_SinkVChanName:
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)COChan->baseClass.sinkVChan, newName);
			OKfree(newName);
			
			break;
	}
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

/// HIFN Obtains a list of physical channel properties for a given DAQmx channel attribute that returns an array of properties.
/// HIPAR chanName/ Physical channel name, e.g. dev1/ai0
/// HIPAR chanIOAttribute/ Physical channel property returning an array such as: DAQmx_PhysicalChan_AI_SupportedMeasTypes, DAQmx_PhysicalChan_AO_SupportedOutputTypes,etc.
static ListType	GetPhysChanPropertyList	(char chanName[], int chanProperty)
{
	ListType 	propertyList 	= ListCreate(sizeof(int));
	int			nElem			= 0;
	int*		properties		= NULL;
	
	if (!propertyList) return 0;
	
	nElem = DAQmxGetPhysicalChanAttribute(chanName, chanProperty, NULL); 
	if (!nElem) return propertyList;
	if (nElem < 0) goto Error;
	
	properties = malloc (nElem * sizeof(int));
	if (!properties) goto Error; // also if nElem = 0, i.e. no properties found
	
	if (DAQmxGetPhysicalChanAttribute(chanName, chanProperty, properties, nElem) < 0) goto Error;
	
	for (size_t i = 0; i < nElem; i++)
		ListInsertItem(propertyList, &properties[i], END_OF_LIST);
	
	OKfree(properties);
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
	AIChannel_type**			AIChanPtr;
	AOChannel_type**   			AOChanPtr;
	DILineChannel_type**		DILineChanPtr;
	DIPortChannel_type**  		DIPortChanPtr;
	DOLineChannel_type**		DOLineChanPtr;
	DOPortChannel_type**  		DOPortChanPtr;
	CIChannel_type**			CIChanPtr;
	COChannel_type**   			COChanPtr;
	size_t						n				= 0;
	
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
						AIChanPtr = ListGetPtrToItem(dev->attr->AIchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*AIChanPtr)->supportedMeasTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*AIChanPtr)->inUse) {
							shortChName = strstr((*AIChanPtr)->physChanName, "/") + 1;  
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*AIChanPtr)->physChanName);
						}
					}
					
					break;
					
				case DAQmxDigital:
					
					switch (ioType) {
						
						case DAQmxDigLines:
							
							n = ListNumItems(dev->attr->DIlines);
							for (int i = 1; i <= n; i++) {
								DILineChanPtr = ListGetPtrToItem(dev->attr->DIlines, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DILineChanPtr)->inUse) {
									shortChName = strstr((*DILineChanPtr)->physChanName, "/") + 1;  
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DILineChanPtr)->physChanName);
								}
							}
							
							break;
							
						case DAQmxDigPorts:
							
							n = ListNumItems(dev->attr->DIports);
							for (int i = 1; i <= n; i++) {
								DIPortChanPtr = ListGetPtrToItem(dev->attr->DIports, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DIPortChanPtr)->inUse) {
									shortChName = strstr((*DIPortChanPtr)->physChanName, "/") + 1;  
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DIPortChanPtr)->physChanName);
								}
							}
							
							break;
					}
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->CIchan);
					for (int i = 1; i <= n; i++) {
						CIChanPtr = ListGetPtrToItem(dev->attr->CIchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*CIChanPtr)->supportedMeasTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*CIChanPtr)->inUse) {
							shortChName = strstr((*CIChanPtr)->physChanName, "/") + 1; 
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*CIChanPtr)->physChanName);
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
						AOChanPtr = ListGetPtrToItem(dev->attr->AOchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*AOChanPtr)->supportedOutputTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*AOChanPtr)->inUse) {
							shortChName = strstr((*AOChanPtr)->physChanName, "/") + 1;
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*AOChanPtr)->physChanName);
						}
					}
					
					break;
					
				case DAQmxDigital:
					
						switch (ioType) {
						
						case DAQmxDigLines:
							
							n = ListNumItems(dev->attr->DOlines);
							for (int i = 1; i <= n; i++) {
								DOLineChanPtr = ListGetPtrToItem(dev->attr->DOlines, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DOLineChanPtr)->inUse) {
									shortChName = strstr((*DOLineChanPtr)->physChanName, "/") + 1;
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DOLineChanPtr)->physChanName);
								}
							}
							
							break;
							
						case DAQmxDigPorts:
							
							n = ListNumItems(dev->attr->DOports);
							for (int i = 1; i <= n; i++) {
								DOPortChanPtr = ListGetPtrToItem(dev->attr->DOports, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!(*DOPortChanPtr)->inUse) {
									shortChName = strstr((*DOPortChanPtr)->physChanName, "/") + 1;
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*DOPortChanPtr)->physChanName);
								}
							}
							
							break;
					}
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->COchan);
					for (int i = 1; i <= n; i++) {
						COChanPtr = ListGetPtrToItem(dev->attr->COchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem ((*COChanPtr)->supportedOutputTypes, &ioType, FRONT_OF_LIST, IntCompare) && !(*COChanPtr)->inUse) {
							shortChName = strstr((*COChanPtr)->physChanName, "/") + 1;
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, (*COChanPtr)->physChanName);
						}
					}
					
					break;
			}
			
			break;
	}
	
}

static int AddDAQmxChannel (Dev_type* dev, DAQmxIO_type ioVal, DAQmxIOMode_type ioMode, int ioType, char chName[], char** errorInfo)
{
#define AddDAQmxChannel_Err_TaskConfigFailed	-1
	char* 		newVChanName	= NULL;	
	int 		panHndl;
	int    		newTabIdx;
	void*   	callbackData	= NULL; 
	int 		chanPanHndl; 
	int			error			= 0;
	char*		errMsg			= NULL;
	char* 		shortChanName	= NULL;
	
	switch (ioVal) {
		
		case DAQmxAcquire: 
			
			switch (ioMode) {
			
				case DAQmxAnalog:
					
					// name of VChan is unique because because task controller names are unique and physical channel names are unique
					newVChanName = GetTaskControlName(dev->taskController);
					AppendString(&newVChanName, ": ", -1);
					AppendString(&newVChanName, chName, -1);
					
					//-------------------------------------------------
					// if there is no AI task then create it and add UI
					//-------------------------------------------------
							
					if(!dev->AITaskSet) {
						// init AI task structure data
						dev->AITaskSet = init_ADTaskSet_type(dev);
						newUI_ADTaskSet(dev->AITaskSet, DAQmxAITaskSetTabName, RemoveDAQmxAIChannel_CB, dev->attr->AITriggerUsage, SinkVChan_AINSamples_BaseName, 
										SourceVChan_AINSamples_BaseName, SinkVChan_AISamplingRate_BaseName, SourceVChan_AISamplingRate_BaseName, HWTrig_AI_BaseName);
					}
							
					//------------------------------------------------
					// add new channel
					//------------------------------------------------
					AIChannel_type* AIchanAttr 	= GetAIChannel(dev, chName);
					
					
					// mark channel as in use
					AIchanAttr->inUse = TRUE;
							
					int chanPanHndl;
					GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_ChanTabIdx, &chanPanHndl);
									
					switch (ioType) {
								
						//--------------------------------------
						// Voltage measurement
						//--------------------------------------
						case DAQmx_Val_Voltage:			
							
							// init new channel data
							ChanSet_AIAO_Voltage_type* newChan 	= (ChanSet_AIAO_Voltage_type*) init_ChanSet_AIAO_Voltage_type(dev, chName, Chan_AI_Voltage, AIchanAttr, NULL);
							
							// insert new channel settings tab
							int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AIAOChSet);  
							int newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
							// change tab title
							shortChanName = strstr(chName, "/") + 1;  
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
							
							//------------------
							// integration mode
							//------------------
							
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_Integrate, newChan->baseClass.integrateFlag);
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_Integrate, ATTR_VISIBLE, TRUE);
							
							//---------------------------
							// adjust data type scaling
							//---------------------------
							// offset
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_Offset, newChan->baseClass.offset); 
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_Offset, ATTR_VISIBLE, TRUE);
							// gain
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_Gain, newChan->baseClass.gain); 
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_Gain, ATTR_VISIBLE, TRUE);
							// max value
							newChan->baseClass.ScaleMax = newChan->Vmax;
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_ScaleMax, newChan->baseClass.ScaleMax); 
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_ScaleMax, ATTR_MAX_VALUE, newChan->Vmax);
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_ScaleMax, ATTR_MIN_VALUE, newChan->Vmin);
							// min value
							newChan->baseClass.ScaleMin = newChan->Vmin;
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_ScaleMin, newChan->baseClass.ScaleMin); 
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_ScaleMin, ATTR_MAX_VALUE, newChan->Vmax);
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_ScaleMin, ATTR_MIN_VALUE, newChan->Vmin);
							
							
							//---------------------
							// Data type conversion
							//---------------------
							InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_AIDataType, AI_Double, "double", AI_Double);
							InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_AIDataType, AI_Float, "float", AI_Float);
							InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_AIDataType, AI_UInt, "uInt", AI_UInt);
							InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_AIDataType, AI_UShort, "uShort", AI_UShort);
							InsertListItem(newChan->baseClass.chanPanHndl, AIAOChSet_AIDataType, AI_UChar, "uChar", AI_UChar);
							SetCtrlIndex(newChan->baseClass.chanPanHndl, AIAOChSet_AIDataType, AI_Double); // set double to default
							SetCtrlAttribute(newChan->baseClass.chanPanHndl, AIAOChSet_AIDataType, ATTR_VISIBLE, TRUE);
							
							//--------------------------
							// Create and register VChan
							//--------------------------
							
							newChan->baseClass.srcVChan = init_SourceVChan_type(newVChanName, DL_Waveform_Double, newChan, NULL, NULL);  
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
					
					//--------------------------------------  
					// Configure AI task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxAITask(dev, &errMsg) );
					
					break;
							
			
				case DAQmxDigital:
					
					// name of VChan is unique because because task controller names are unique and physical channel names are unique
					newVChanName = GetTaskControlName(dev->taskController);
					AppendString(&newVChanName, ": ", -1);
					AppendString(&newVChanName, chName, -1);
					
					//-------------------------------------------------
					// if there is no DI task then create it and add UI
					//-------------------------------------------------
							
					if(!dev->DITaskSet) {
						// init DI task structure data
						dev->DITaskSet = init_ADTaskSet_type(dev);
						newUI_ADTaskSet(dev->DITaskSet, DAQmxDITaskSetTabName, RemoveDAQmxDIChannel_CB, dev->attr->DITriggerUsage, SinkVChan_DINSamples_BaseName, 
										SourceVChan_DINSamples_BaseName, SinkVChan_DISamplingRate_BaseName, SourceVChan_DISamplingRate_BaseName, HWTrig_DI_BaseName);
					}
					
					//--------------------------------------  
					// Configure DI task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxDITask(dev, &errMsg) );
			
					break;
			
				case DAQmxCounter:
					
					// name of VChan is unique because because task controller names are unique and physical channel names are unique
					newVChanName = GetTaskControlName(dev->taskController);
					AppendString(&newVChanName, ": ", -1);
					AppendString(&newVChanName, chName, -1);
					
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
						 //GetPanelHandleFromTabPage(dev->CITaskSet->panHndl, CICOTskSet_Tab, DAQmxCICOTskSet_ChanTabIdx, &chanPanHndl);
						// remove "None" labelled channel tab
						// DeleteTabPage(chanPanHndl, Chan_ChanSet, 0, 1);
						// add callback data and callback function to remove channels
						 //SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_FUNCTION_POINTER, RemoveDAQmxCIChannel_CB);
						 //SetCtrlAttribute(chanPanHndl, Chan_ChanSet,ATTR_CALLBACK_DATA, dev);
							    
						//------------------------------------------------
						// add new channel
						//------------------------------------------------
						
						CIChannel_type* CIchanAttr 	= GetCIChannel(dev, chName);
						// mark channel as in use
						CIchanAttr->inUse = TRUE;
							
						//		GetPanelHandleFromTabPage(dev->CITaskSet->panHndl, CICOTskSet_Tab, DAQmxCICOTskSet_ChanTabIdx, &chanPanHndl);
							
						CIEdgeChanSet_type* newChan 	=  (CIEdgeChanSet_type*) init_ChanSet_CI_Frequency_type(dev, chName);
							
						// insert new channel settings tab
						int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOChSet);  
						newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
						// change tab title
						shortChanName = strstr(chName, "/") + 1;  
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
							
						newChan->baseClass.srcVChan = init_SourceVChan_type(newVChanName, DL_Waveform_Double, newChan, NULL, NULL);  
						DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newChan->baseClass.srcVChan);
						SetCtrlVal(newChan->baseClass.chanPanHndl, SETPAN_SrcVChanName, newVChanName);
						OKfree(newVChanName);
							
						//---------------------------------------
						// Add new CI channel to list of channels
						//---------------------------------------
						ListInsertItem(dev->CITaskSet->chanTaskSet, &newChan, END_OF_LIST);
			
					}
					
					//--------------------------------------  
					// Configure CI tasks on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxCITask(dev, &errMsg) );
					
					break;
			
				case DAQmxTEDS:
			
					break;
			}
			break;
			
		case DAQmxGenerate:
			
			switch (ioMode) {
			
				case DAQmxAnalog:
					
					// name of VChan is unique because because task controller names are unique and physical channel names are unique
					newVChanName = GetTaskControlName(dev->taskController);
					AppendString(&newVChanName, ": ", -1);
					AppendString(&newVChanName, chName, -1);
					
					//-------------------------------------------------
					// if there is no AO task then create it and add UI
					//-------------------------------------------------
							
					if(!dev->AOTaskSet) {
						// init AO task structure data
						dev->AOTaskSet = init_ADTaskSet_type(dev);
						newUI_ADTaskSet(dev->AOTaskSet, DAQmxAOTaskSetTabName, RemoveDAQmxAOChannel_CB, dev->attr->AOTriggerUsage, SinkVChan_AONSamples_BaseName, 
										SourceVChan_AONSamples_BaseName, SinkVChan_AOSamplingRate_BaseName, SourceVChan_AOSamplingRate_BaseName, HWTrig_AO_BaseName); 
					}
							
					//------------------------------------------------
					// add new channel
					//------------------------------------------------
					
					AOChannel_type* AOchanAttr 	= GetAOChannel(dev, chName);
				
					// mark channel as in use
					AOchanAttr->inUse = TRUE;
							
					
					GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_ChanTabIdx, &chanPanHndl);
					
					switch (ioType) {
						
						//--------------------------------------
						// Voltage measurement
						//--------------------------------------
						case DAQmx_Val_Voltage:
							
							// init new channel data
							ChanSet_AIAO_Voltage_type* newChan 	= (ChanSet_AIAO_Voltage_type*) init_ChanSet_AIAO_Voltage_type(dev, chName, Chan_AO_Voltage, NULL, AOchanAttr);
							
							// insert new channel settings tab
							int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AIAOChSet);  
							int newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
							// change tab title
							shortChanName = strstr(chName, "/") + 1;  
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
							
							DLDataTypes allowedPacketTypes[] = {DL_Waveform_Double, DL_RepeatedWaveform_Double, DL_Waveform_Float, DL_RepeatedWaveform_Float, DL_Float, DL_Double};
							
							newChan->baseClass.sinkVChan = init_SinkVChan_type(newVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), newChan, VChan_Data_Receive_Timeout, NULL, NULL);  
							DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newChan->baseClass.sinkVChan);
							SetCtrlVal(newChan->baseClass.chanPanHndl, AIAOChSet_VChanName, newVChanName);
							AddSinkVChan(dev->taskController, newChan->baseClass.sinkVChan, AO_DataReceivedTC); 
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
					
					//--------------------------------------  
					// Configure AO task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxAOTask(dev, &errMsg) );
					
					break;
			
				case DAQmxDigital:
					
					
					// name of VChan is unique because task controller names are unique and physical channel names are unique
					newVChanName = GetTaskControlName(dev->taskController);
					AppendString(&newVChanName, ": ", -1);
					AppendString(&newVChanName, chName, -1);
				
					//-------------------------------------------------
					// if there is no DO task then create it and add UI
					//-------------------------------------------------
						
					if(!dev->DOTaskSet) {
						// init DO task structure data
						dev->DOTaskSet = init_ADTaskSet_type(dev);
						newUI_ADTaskSet(dev->DOTaskSet, DAQmxDOTaskSetTabName, RemoveDAQmxDOChannel_CB, dev->attr->DOTriggerUsage, SinkVChan_DONSamples_BaseName, 
										SourceVChan_DONSamples_BaseName, SinkVChan_DOSamplingRate_BaseName, SourceVChan_DOSamplingRate_BaseName, HWTrig_DO_BaseName); 
					}
						  
					//------------------------------------------------
					// add new channel
					//------------------------------------------------
					uInt32 		data;
					
					GetPanelHandleFromTabPage(dev->DOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_ChanTabIdx, &chanPanHndl);
					     
					int 				ioType;
					int 				newTabIdx;
					ChanSet_DIDO_type* 	newDOChan		= NULL;
					
					GetCtrlVal(dev->devPanHndl, TaskSetPan_IOType, &ioType); 
					
					//set channel type 
					switch (ioType){
						
						case DAQmxDigLines:
						
							newDOChan = init_ChanSet_DIDO_type (dev, chName, ioType);  
							newDOChan->baseClass.chanType = Chan_DO_Line;
							DOLineChannel_type* DOlineChanAttr = GetDOLineChannel (dev, chName);
							// mark channel as in use 
							DOlineChanAttr->inUse = TRUE;
							int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, DIDOLChSet);    
							newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl);
							// change tab title
							shortChanName = strstr(chName, "/") + 1;  
							SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
							DiscardPanel(chanSetPanHndl); 
							chanSetPanHndl = 0;
							GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &newDOChan->baseClass.chanPanHndl);
							SetCtrlVal(newDOChan->baseClass.chanPanHndl, DIDOLChSet_VChanName, newVChanName);  
							// add callbackdata to the channel panel
							SetPanelAttribute(newDOChan->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, newDOChan);
							// add callback data to the controls in the panel
							SetCtrlsInPanCBInfo(newDOChan, ChanSetLineDO_CB, newDOChan->baseClass.chanPanHndl);
							// read current output state and set the control properly
							errChk( ReadDirectDigitalOut(newDOChan->baseClass.name, &data) );
							SetCtrlVal(newDOChan->baseClass.chanPanHndl,DIDOLChSet_OutputBTTN,data);           
							break;
								
						case DAQmxDigPorts:
							
							newDOChan =init_ChanSet_DIDO_type (dev, chName, ioType);  
							newDOChan->baseClass.chanType=Chan_DO_Port;  
							DOPortChannel_type* DOportChanAttr=GetDOPortChannel (dev, chName);
							// mark channel as in use 
							DOportChanAttr->inUse = TRUE; 
							chanSetPanHndl= LoadPanel(0, MOD_NIDAQmxManager_UI, DIDOPChSet);    
							newTabIdx= InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
							shortChanName = strstr(chName, "/") + 1;  
							SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
							DiscardPanel(chanSetPanHndl); 
							chanSetPanHndl = 0;
							GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &newDOChan->baseClass.chanPanHndl);
							SetCtrlVal(newDOChan->baseClass.chanPanHndl, DIDOPChSet_VChanName, newVChanName);  
							// add callbackdata to the channel panel
							SetPanelAttribute(newDOChan->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, newDOChan);
							// add callback data to the controls in the panel
							SetCtrlsInPanCBInfo(newDOChan, ChanSetPortDO_CB, newDOChan->baseClass.chanPanHndl);
						
							//read current output state and set the control properly
							errChk( ReadDirectDigitalOut(newDOChan->baseClass.name,&data) );
							SetPortControls(newDOChan->baseClass.chanPanHndl,data);  
						
							break;
					}
				    
					
					// insert new channel settings tab
					
					//-------------------------------------------------------------
					// Create and register VChan with Task Controller and framework
					//-------------------------------------------------------------
							
					DLDataTypes allowedPacketTypes[] = {	
						DL_Waveform_Char,
						DL_Waveform_UChar,
						DL_Waveform_Short,
						DL_Waveform_UShort,
						DL_Waveform_Int,
						DL_Waveform_UInt,
						DL_RepeatedWaveform_Char,						
						DL_RepeatedWaveform_UChar,						
						DL_RepeatedWaveform_Short,						
						DL_RepeatedWaveform_UShort,					
						DL_RepeatedWaveform_Int,						
						DL_RepeatedWaveform_UInt};
						
					newDOChan->baseClass.sinkVChan = init_SinkVChan_type(newVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), newDOChan, VChan_Data_Receive_Timeout, NULL, NULL);  
					DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newDOChan->baseClass.sinkVChan);
					
					AddSinkVChan(dev->taskController, newDOChan->baseClass.sinkVChan, DO_DataReceivedTC); 
					OKfree(newVChanName);
							
					//---------------------------------------
					// Add new DO channel to list of channels
					//---------------------------------------
					ListInsertItem(dev->DOTaskSet->chanSet, &newDOChan, END_OF_LIST);
				
					//--------------------------------------  
					// Configure DO task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxDOTask(dev, &errMsg) );
					
					break;
					
				case DAQmxCounter:
					
					//-------------------------------------------------
					// if there is no CO task then create it and add UI
					//-------------------------------------------------
							
					if(!dev->COTaskSet) {
						// init CO task structure data
						dev->COTaskSet = init_COTaskSet_type();    
						
						// load UI resources
						int CICOTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOTskSet);  
						// insert panel to UI and keep track of the AI task settings panel handle
						int newTabIdx = InsertPanelAsTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, CICOTaskSetPanHndl); 
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, &dev->COTaskSet->panHndl);
						// change tab title to new Task Controller name
						SetTabPageAttribute(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, ATTR_LABEL_TEXT, DAQmxCOTaskSetTabName);
				
						// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
							
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, &panHndl);
						GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
						if (!callbackData) DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, 1);
						// connect CO task settings data to the panel
						SetPanelAttribute(dev->COTaskSet->panHndl, ATTR_CALLBACK_DATA, dev->COTaskSet);
							
						// add callback data and callback function to remove channels
						SetCtrlAttribute(dev->COTaskSet->panHndl, Chan_ChanSet,ATTR_CALLBACK_FUNCTION_POINTER, RemoveDAQmxCOChannel_CB);
						SetCtrlAttribute(dev->COTaskSet->panHndl, Chan_ChanSet,ATTR_CALLBACK_DATA, dev);
						
					}
						
							
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
							
					Channel_type 			chanType;
					PulseTrainTimingTypes 	pulseTrainType	= PulseTrain_Freq;
							
					GetCtrlVal(dev->devPanHndl, TaskSetPan_IOType, &ioType);  
					//set channel type 
					switch (ioType){
						
						case DAQmx_Val_Pulse_Freq:
							chanType		= Chan_CO_Pulse_Frequency;
							pulseTrainType	= PulseTrain_Freq;
							break;
									
						case DAQmx_Val_Pulse_Time:
							chanType		= Chan_CO_Pulse_Time;
							pulseTrainType	= PulseTrain_Time;  
							break;
									
						case DAQmx_Val_Pulse_Ticks:
							chanType		= Chan_CO_Pulse_Ticks;
							pulseTrainType	= PulseTrain_Ticks;  
							break;
					}
					
					ChanSet_CO_type* newChan 	=  (ChanSet_CO_type*) init_ChanSet_CO_type(dev, chName, pulseTrainType);
						
					// insert new channel tab
					int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOChSet);  
					newTabIdx = InsertPanelAsTabPage(dev->COTaskSet->panHndl, Chan_ChanSet, -1, chanSetPanHndl); 
					// change tab title
					shortChanName = strstr(chName, "/") + 1;  
					
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
					GetCtrlVal(settingsPanHndl, SETPAN_Timeout, &dev->COTaskSet->timeout);
						
					// add callback to controls in the panel
					SetCtrlsInPanCBInfo(newChan, ChanSetCO_CB, settingsPanHndl);   
								
					//--------------------------
					// adjust "Timing" tab
					//--------------------------
					
					// delete empty Timing tab
					DeleteTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, 1); 
					
					int 					timingPanHndl;	
					int 					COPulsePanHndl	= 0;
					double					time;
					PulseTrainIdleStates	idleState;
					int 					ctrlIdx;
					
					switch (chanType){
						
						case Chan_CO_Pulse_Frequency:
							// load panel
							COPulsePanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, COFreqPan);
							InsertPanelAsTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, COPulsePanHndl);
							DiscardPanel(COPulsePanHndl);
							SetTabPageAttribute(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, ATTR_LABEL_TEXT, "Timing");
							GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl);
							
							// add control callbacks
							SetCtrlsInPanCBInfo(newChan, Chan_CO_Pulse_Frequency_CB, timingPanHndl);
							
							// init idle state
							idleState = GetPulseTrainIdleState(newChan->pulseTrain);
							switch (idleState) {
								
								case PulseTrainIdle_Low:
									SetCtrlIndex(timingPanHndl, COFreqPan_IdleState, 0);
									break;
								case PulseTrainIdle_High:
									SetCtrlIndex(timingPanHndl, COFreqPan_IdleState, 1);
									break;
							}
							
							// init frequency
							SetCtrlVal(timingPanHndl, COFreqPan_Frequency, GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)newChan->pulseTrain)); 					// display in [Hz]
							
							// init duty cycle
							SetCtrlVal(timingPanHndl, COFreqPan_DutyCycle, GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)newChan->pulseTrain)); 				// display in [%]
							
							// init initial delay
							SetCtrlVal(timingPanHndl, COFreqPan_InitialDelay, GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)newChan->pulseTrain)*1e3); 	// display in [ms]
							
							// init CO generation mode
							InsertListItem(timingPanHndl, COFreqPan_Mode, -1, "Finite", MeasFinite);
							InsertListItem(timingPanHndl, COFreqPan_Mode, -1, "Continuous", MeasCont);
							GetIndexFromValue(timingPanHndl, COFreqPan_Mode, &ctrlIdx, GetPulseTrainMode(newChan->pulseTrain)); 
							SetCtrlIndex(timingPanHndl, COFreqPan_Mode, ctrlIdx);
							
							// init CO N Pulses
							SetCtrlVal(timingPanHndl, COFreqPan_NPulses, GetPulseTrainNPulses(newChan->pulseTrain)); 
							
							break;
									
						case Chan_CO_Pulse_Time:
							
							COPulsePanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, COTimePan);
							InsertPanelAsTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, COPulsePanHndl);
							DiscardPanel(COPulsePanHndl);
							SetTabPageAttribute(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, ATTR_LABEL_TEXT, "Timing");
							GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl);
							
							// add control callbacks
							SetCtrlsInPanCBInfo(newChan, Chan_CO_Pulse_Time_CB, timingPanHndl);
							
							// init idle state
							idleState = GetPulseTrainIdleState(newChan->pulseTrain);
							switch (idleState) {
								
								case PulseTrainIdle_Low:
									SetCtrlIndex(timingPanHndl, COTimePan_IdleState, 0);
									break;
								case PulseTrainIdle_High:
									SetCtrlIndex(timingPanHndl, COTimePan_IdleState, 1);
									break;
							}
							
							// init high time
							SetCtrlVal(timingPanHndl, COTimePan_HighTime, GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)newChan->pulseTrain)*1e3); 			// display in [ms]
							
							// init low time
							SetCtrlVal(timingPanHndl, COTimePan_LowTime, GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)newChan->pulseTrain)*1e3); 				// display in [ms]
							
							// init initial delay
							SetCtrlVal(timingPanHndl, COTimePan_InitialDelay, GetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)newChan->pulseTrain)*1e3); 	// display in [ms]
							
							// init CO generation mode
							InsertListItem(timingPanHndl, COTimePan_Mode, -1, "Finite", MeasFinite);
							InsertListItem(timingPanHndl, COTimePan_Mode, -1, "Continuous", MeasCont);
							GetIndexFromValue(timingPanHndl, COTimePan_Mode, &ctrlIdx, GetPulseTrainMode(newChan->pulseTrain)); 
							SetCtrlIndex(timingPanHndl, COTimePan_Mode, ctrlIdx);
							
							// init CO N Pulses
							SetCtrlVal(timingPanHndl, COTimePan_NPulses, GetPulseTrainNPulses(newChan->pulseTrain)); 
									
							break;
									
						case Chan_CO_Pulse_Ticks:
							
							COPulsePanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, COTicksPan);
							InsertPanelAsTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, COPulsePanHndl);
							DiscardPanel(COPulsePanHndl);
							SetTabPageAttribute(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, ATTR_LABEL_TEXT, "Timing");
							GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl);
							
							// add control callbacks
							SetCtrlsInPanCBInfo(newChan, Chan_CO_Pulse_Ticks_CB, timingPanHndl);
							
							// init idle state
							idleState = GetPulseTrainIdleState(newChan->pulseTrain);
							switch (idleState) {
								
								case PulseTrainIdle_Low:
									SetCtrlIndex(timingPanHndl, COTicksPan_IdleState, 0);
									break;
								case PulseTrainIdle_High:
									SetCtrlIndex(timingPanHndl, COTicksPan_IdleState, 1);
									break;
							}
							
							// init high ticks
							SetCtrlVal(timingPanHndl, COTicksPan_HighTicks, GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)newChan->pulseTrain));
							
							// init low ticks
							SetCtrlVal(timingPanHndl, COTicksPan_LowTicks, GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)newChan->pulseTrain));
							
							// init initial delay ticks
							SetCtrlVal(timingPanHndl, COTicksPan_InitialDelay, GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)newChan->pulseTrain));   
							
							// init CO generation mode
							InsertListItem(timingPanHndl, COTicksPan_Mode, -1, "Finite", MeasFinite);
							InsertListItem(timingPanHndl, COTicksPan_Mode, -1, "Continuous", MeasCont);
							GetIndexFromValue(timingPanHndl, COTicksPan_Mode, &ctrlIdx, GetPulseTrainMode(newChan->pulseTrain)); 
							SetCtrlIndex(timingPanHndl, COTicksPan_Mode, ctrlIdx);
							
							// init CO N Pulses
							SetCtrlVal(timingPanHndl, COTicksPan_NPulses, GetPulseTrainNPulses(newChan->pulseTrain)); 
							
							break;
					}

					//--------------------------
					// adjust "Clk" tab
					//--------------------------
					// get timing tab panel handle
						
					// add trigger data structure
						
					newChan->baseClass.referenceTrig = init_TaskTrig_type(dev, 0);	   //? lex
					
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
						
					// set default reference clock source
					newChan->baseClass.referenceTrig->trigSource = StrDup("OnboardClock");
					SetCtrlVal(clkPanHndl, CLKPAN_RefClkSource, "OnboardClock");
		
					// add callback to controls in the panel
					SetCtrlsInPanCBInfo(newChan, Chan_CO_Clk_CB, clkPanHndl);
						
					//--------------------------
					// adjust "Trigger" tab
					//--------------------------
					// get trigger tab panel handle
					newChan->baseClass.startTrig = init_TaskTrig_type(dev, 0);  //? lex  
						
					int trigPanHndl;
					GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TriggerTabIdx, &trigPanHndl);
								
					// make sure that the host controls are not dimmed before inserting terminal controls!
					NIDAQmx_NewTerminalCtrl(trigPanHndl, TrigPan_Source, 0); // single terminal selection
					
								
					// adjust sample clock terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(trigPanHndl, TrigPan_Source, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
					NIDAQmx_SetTerminalCtrlAttribute(trigPanHndl, TrigPan_Source, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
						
					// set default start trigger source
					newChan->baseClass.startTrig->trigSource = StrDup("/Dev2/ao/StartTrigger");
					SetCtrlVal(trigPanHndl, TrigPan_Source, "/Dev2/ao/StartTrigger");
						
					// insert trigger type options
					InsertListItem(trigPanHndl, TrigPan_TrigType, -1, "None", Trig_None); 
					InsertListItem(trigPanHndl, TrigPan_TrigType, -1, "Digital Edge", Trig_DigitalEdge); 
					newChan->baseClass.startTrig->trigType=Trig_None;
					SetCtrlAttribute(trigPanHndl,TrigPan_Slope,ATTR_DIMMED,TRUE);
					SetCtrlAttribute(trigPanHndl,TrigPan_Source,ATTR_DIMMED,TRUE); 									   ;  
						
					InsertListItem(trigPanHndl,TrigPan_Slope , 0, "Rising", TrigSlope_Rising); 
					InsertListItem(trigPanHndl,TrigPan_Slope , 1, "Falling", TrigSlope_Falling);
					newChan->baseClass.startTrig->slope = TrigSlope_Rising;
						
					// add callback to controls in the panel
					SetCtrlsInPanCBInfo(newChan, Chan_CO_Trig_CB, trigPanHndl);
						
					//------------------------------------
					// Create and register CO Source VChan
					//------------------------------------
						
					int settingspanel;
					GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB,DAQmxCICOTskSet_SettingsTabIdx , &settingspanel);    
					
					// add a sourceVChan to send pulsetrain settings
					// name of VChan is unique because because task controller names are unique and physical channel names are unique
					
					char* pulseTrainSourceVChanName	= GetTaskControlName(dev->taskController);
					AppendString(&pulseTrainSourceVChanName, ": ", -1);
					AppendString(&pulseTrainSourceVChanName	, chName, -1);
					AppendString(&pulseTrainSourceVChanName, SourceVChan_COPulseTrain_BaseName, -1);
					
					switch (GetPulseTrainType(newChan->pulseTrain)) {
						
						case PulseTrain_Freq:
							newChan->baseClass.srcVChan 	= init_SourceVChan_type(pulseTrainSourceVChanName, DL_PulseTrain_Freq, newChan, 
															  						COPulseTrainSourceVChan_Connected, NULL); 
							break;
								
						case PulseTrain_Time:
							newChan->baseClass.srcVChan		= init_SourceVChan_type(pulseTrainSourceVChanName, DL_PulseTrain_Time, newChan, 
														  						COPulseTrainSourceVChan_Connected, NULL); 
							break;
								
						case PulseTrain_Ticks:
							newChan->baseClass.srcVChan 	= init_SourceVChan_type(pulseTrainSourceVChanName, DL_PulseTrain_Ticks, newChan, 
																  						COPulseTrainSourceVChan_Connected, NULL); 
							break;
							
					}
					
					SetCtrlVal(settingspanel, SETPAN_SrcVChanName, pulseTrainSourceVChanName);  	
					// register Source VChan with DAQLab
					DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) newChan->baseClass.srcVChan);
					
					// cleanup
					OKfree(pulseTrainSourceVChanName);  
					
					//------------------------------------
					// Create and register CO Sink VChan
					//------------------------------------
					
					char* pulseTrainSinkVChanName	= GetTaskControlName(dev->taskController);
					AppendString(&pulseTrainSinkVChanName, ": ", -1);
					AppendString(&pulseTrainSinkVChanName, chName, -1);
					AppendString(&pulseTrainSinkVChanName, " ", -1);
					AppendString(&pulseTrainSinkVChanName, SinkVChan_COPulseTrain_BaseName, -1);
					
					DLDataTypes allowedPulseTrainPacket;
					
					switch (GetPulseTrainType(newChan->pulseTrain)) {
						
						case PulseTrain_Freq:
							allowedPulseTrainPacket = DL_PulseTrain_Freq;
							break;
								
						case PulseTrain_Time:
							allowedPulseTrainPacket = DL_PulseTrain_Time; 
							break;
								
						case PulseTrain_Ticks:
							allowedPulseTrainPacket = DL_PulseTrain_Ticks; 
							break;
					}
					
					newChan->baseClass.sinkVChan	= init_SinkVChan_type(pulseTrainSinkVChanName, &allowedPulseTrainPacket, 1, 
																					 newChan, VChan_Data_Receive_Timeout, COPulseTrainSinkVChan_Connected, COPulseTrainSinkVChan_Disconnected); 
					
					SetCtrlVal(settingspanel, SETPAN_SinkVChanName, pulseTrainSinkVChanName);  
					// register VChan with DAQLab
					DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) newChan->baseClass.sinkVChan);	
					AddSinkVChan(dev->taskController, newChan->baseClass.sinkVChan, CO_DataReceivedTC); 
					
					// cleanup
					OKfree(pulseTrainSinkVChanName);
					
					//-------------------------------------------------------------------------
					// add HW Triggers
					//-------------------------------------------------------------------------
						
					// Master & Slave HW Triggers
					char*	HWTrigName = GetTaskControlName(dev->taskController);
					AppendString(&HWTrigName, ": ", -1);
					AppendString(&HWTrigName, chName, -1); 
					AppendString(&HWTrigName, " ", -1);
					AppendString(&HWTrigName, HWTrig_CO_BaseName, -1);
					newChan->baseClass.HWTrigMaster 	= init_HWTrigMaster_type(HWTrigName);
					newChan->baseClass.HWTrigSlave		= init_HWTrigSlave_type(HWTrigName);
					OKfree(HWTrigName);
						
					// register HW Triggers with framework
					DLRegisterHWTrigMaster(newChan->baseClass.HWTrigMaster);
					DLRegisterHWTrigSlave(newChan->baseClass.HWTrigSlave);
					
					//---------------------------------------
					// Add new CO channel to list of channels
					//---------------------------------------
					ListInsertItem(dev->COTaskSet->chanTaskSet, &newChan, END_OF_LIST);
					
					//--------------------------------------  
					// Configure CO task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxCOTask(dev, &errMsg) );
					
					break;
			
				case DAQmxTEDS:
			
					break;
			}
		}
	
	return 0;

Error:

	if (errorInfo)
		*errorInfo = FormatMsg(AddDAQmxChannel_Err_TaskConfigFailed, "AddDAQmxChannel", errMsg);
	OKfree(errMsg);

	return error;
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
			ChanSet_type** 	chanSetPtr;
			size_t			nChans			= ListNumItems(dev->AITaskSet->chanSet);
			for (size_t i = 1; i <= nChans; i++) {	
				chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
				if (*chanSetPtr == aiChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->srcVChan);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	(chanSetPtr);
					ListRemoveItem(dev->AITaskSet->chanSet, 0, i);
					break;
				}
			}
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove AI task
			if (!nTabs) {
				
				discardUI_ADTaskSet(&dev->AITaskSet);  
				
				// discard UI
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
			ChanSet_type** 	chanSetPtr;
			size_t			nChans			= ListNumItems(dev->AOTaskSet->chanSet);
			for (size_t i = 1; i <= nChans; i++) {	
				chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
				if (*chanSetPtr == aoChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->sinkVChan);
					// detach from Task Controller										 
					RemoveSinkVChan(dev->taskController, (*chanSetPtr)->sinkVChan);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	(chanSetPtr);
					ListRemoveItem(dev->AOTaskSet->chanSet, 0, i);
					break;
				}
			}
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove AO task
			if (!nTabs) {
				
				discardUI_ADTaskSet(&dev->AOTaskSet);
				
				// discard UI
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

static int RemoveDAQmxDIChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_KEYPRESS: 
			
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			// fall through
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			Dev_type*	dev = callbackData; 
			
			int tabIdx;
			GetActiveTabPage(panel, control, &tabIdx);
			int chanTabPanHndl;
			GetPanelHandleFromTabPage(panel, control, tabIdx, &chanTabPanHndl);
			ChanSet_type* diChanPtr;
			GetPanelAttribute(chanTabPanHndl, ATTR_CALLBACK_DATA, &diChanPtr);
			// if this is the "None" labelled tab stop here
			if (!diChanPtr) break;
			
			// mark channel as available again  
			switch (diChanPtr->chanType){
					
				case Chan_DI_Line:
					DILineChannel_type* DILineChanAttr = GetDILineChannel (dev, diChanPtr->name);
					DILineChanAttr->inUse = FALSE;  
					break;
					
				case Chan_DI_Port:
					DIPortChannel_type* DIPortChanAttr = GetDIPortChannel (dev, diChanPtr->name);
					DIPortChanAttr->inUse = FALSE;
					break;
			}
			
			// remove channel from DI task
			ChanSet_type** 	chanSetPtr;
			size_t			nChans			= ListNumItems(dev->DITaskSet->chanSet);
			for (size_t i = 1; i <= nChans; i++) {	
				chanSetPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
				if (*chanSetPtr == diChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->sinkVChan);
					// detach from Task Controller										 
					RemoveSinkVChan(dev->taskController, (*chanSetPtr)->srcVChan);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	(chanSetPtr);
					ListRemoveItem(dev->DITaskSet->chanSet, 0, i);
					break;
				}
			}		   
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove DI task
			if (!nTabs) {
				
				discardUI_ADTaskSet(&dev->DITaskSet);  
				
				// discard UI
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

static int RemoveDAQmxDOChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event) {
		
		case EVENT_KEYPRESS: 
			
			// continue only if Del key is pressed
			if (eventData1 != VAL_FWD_DELETE_VKEY) break;
			// fall through
			
		case EVENT_LEFT_DOUBLE_CLICK:
			
			Dev_type*	dev = callbackData; 
			
			int tabIdx;
			GetActiveTabPage(panel, control, &tabIdx);
			int chanTabPanHndl;
			GetPanelHandleFromTabPage(panel, control, tabIdx, &chanTabPanHndl);
			ChanSet_type* doChanPtr;
			GetPanelAttribute(chanTabPanHndl, ATTR_CALLBACK_DATA, &doChanPtr);
			// if this is the "None" labelled tab stop here
			if (!doChanPtr) break;
				
			// mark channel as available again  
			switch (doChanPtr->chanType){
					
				case Chan_DO_Line:
					DOLineChannel_type* DOLineChanAttr = GetDOLineChannel (dev, doChanPtr->name);
					DOLineChanAttr->inUse = FALSE;  
					break;
					
				case Chan_DO_Port:
					DOPortChannel_type* DOPortChanAttr = GetDOPortChannel (dev, doChanPtr->name);
					DOPortChanAttr->inUse = FALSE;
					break;
			}
			
			// remove channel from DO task
			ChanSet_type** 	chanSetPtr;
			size_t			nChans			= ListNumItems(dev->DOTaskSet->chanSet);
			for (size_t i = 1; i <= nChans; i++) {	
				chanSetPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
				if (*chanSetPtr == doChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->sinkVChan);
					// detach from Task Controller										 
					RemoveSinkVChan(dev->taskController, (*chanSetPtr)->srcVChan);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	(chanSetPtr);
					ListRemoveItem(dev->DOTaskSet->chanSet, 0, i);
					break;
				}
			}		   
			
			// remove channel tab
			DeleteTabPage(panel, control, tabIdx, 1);
			int nTabs;
			GetNumTabPages(panel, control, &nTabs);
			// if there are no more channels, remove DO task
			if (!nTabs) {
				
				discardUI_ADTaskSet(&dev->DOTaskSet);  
				
				// discard UI
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
	/*		ChanSet_type** 	chanSetPtr;
			size_t			nChans			= ListNumItems(dev->CITaskSet->chanSet);
			size_t			chIdx			= 1;
			for (size_t i = 1; i <= nChans; i++) {	
				chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanSet, i);
				if (*chanSetPtr == ciChanPtr) {
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->sinkVChan);
					// detach from Task Controller										 
					RemoveSinkVChan(dev->taskController, (*chanSetPtr)->sinkVChan);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	(chanSetPtr);
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
			ChanSet_type* coChan;
			GetPanelAttribute(chanTabPanHndl, ATTR_CALLBACK_DATA, &coChan);
			// if this is the "None" labelled tab stop here
			if (!coChan) break;
				
			// mark channel as available again
			COChannel_type* COChanAttr 	= GetCOChannel(dev, coChan->name);
			COChanAttr->inUse = FALSE;
			// remove channel from CO task
			ChanSet_type** 	chanSetPtr;
			size_t			nChans			= ListNumItems(dev->COTaskSet->chanTaskSet);
			for (size_t i = 1; i <= nChans; i++) {	
				chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
				if (*chanSetPtr == coChan) {
					// remove Source VChan from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->srcVChan);
					// remove Sink VChan from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->sinkVChan);
					// detach from Task Controller										 
					RemoveSinkVChan(dev->taskController, (*chanSetPtr)->sinkVChan);
					// remove HW triggers from framework
					DLUnregisterHWTrigMaster((*chanSetPtr)->HWTrigMaster);
					DLUnregisterHWTrigSlave((*chanSetPtr)->HWTrigSlave);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	(chanSetPtr);
					ListRemoveItem(dev->COTaskSet->chanTaskSet, 0, i);
					break;
				}
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

static ListType GetSupportedIOTypes (char devName[], int IOType)
{
	ListType 	IOTypes 	= ListCreate(sizeof(int));
	int			nElem		= 0;
	int*		io			= NULL;
	
	if (!IOTypes) return 0;
	
	if((nElem = DAQmxGetDeviceAttribute(devName, IOType, NULL)) < 0) goto Error;
	
	if (!nElem) return IOTypes; // no suported IO types
	
	io = malloc (nElem * sizeof(int));
	if (!io) goto Error; // also if nElem = 0, i.e. no IO types found
	
	if (DAQmxGetDeviceAttribute(devName, IOType, io, nElem) < 0) goto Error;
	
	for (size_t i = 0; i < nElem; i++)
		ListInsertItem(IOTypes, &io[i], END_OF_LIST);
	
	return IOTypes;
	
Error:
	OKfree(io);
	ListDispose(IOTypes);
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

//------------------------------------------------------------------------------
// IORange_type
//------------------------------------------------------------------------------
static IORange_type* init_IORange_type(void)
{
	IORange_type* IORange = malloc (sizeof(IORange_type));
	if (!IORange) return NULL;
	
	IORange -> Nrngs 	= 0;
	IORange -> low 		= NULL;
	IORange -> high 	= NULL;
	
	return IORange;
}

static void discard_IORange_type(IORange_type** IORangePtr)
{
	if (!*IORangePtr) return;
	
	(*IORangePtr)->Nrngs = 0;
	OKfree((*IORangePtr)->low);
	OKfree((*IORangePtr)->high);
	OKfree(*IORangePtr);
}

static IORange_type* copy_IORange_type (IORange_type* IOrange)
{
	IORange_type* ioRangeCopy = init_IORange_type();
	if (!ioRangeCopy) return NULL;
	
	if (!IOrange->Nrngs) return ioRangeCopy; // no ranges
	// allocate memory for the ranges
	ioRangeCopy->low = malloc(IOrange->Nrngs * sizeof(double));
	if (!ioRangeCopy->low) goto Error;
	ioRangeCopy->high = malloc(IOrange->Nrngs * sizeof(double));
	if (!ioRangeCopy->high) goto Error;
	
	ioRangeCopy->Nrngs = IOrange->Nrngs;
	
	// copy ranges
	memcpy(ioRangeCopy->low, IOrange->low, IOrange->Nrngs * sizeof(double));  
	memcpy(ioRangeCopy->high, IOrange->high, IOrange->Nrngs * sizeof(double));
	
	return ioRangeCopy;
	
Error:
	discard_IORange_type(&ioRangeCopy);
	return NULL;
}

static IORange_type* GetIORanges (char devName[], int rangeType)
{
	IORange_type* 	IORngsPtr	= init_IORange_type();
	int 			nElem		= 0; 
	double* 		rngs     	= NULL;
	
	if (!IORngsPtr) return NULL;
	
	if((nElem = DAQmxGetDeviceAttribute(devName, rangeType, NULL)) < 0) goto Error;
	if (!nElem) return IORngsPtr;  // no ranges available
	
	IORngsPtr->Nrngs = nElem/2;
	rngs = malloc (nElem * sizeof(double));
	if (!rngs) goto Error; 
	
	IORngsPtr->low = malloc((IORngsPtr->Nrngs) * sizeof(double));
	if (!IORngsPtr->low) goto Error;
	
	IORngsPtr->high = malloc((IORngsPtr->Nrngs) * sizeof(double));
	if (!IORngsPtr->high) goto Error;
	
	if (DAQmxGetDeviceAttribute(devName, rangeType, rngs, nElem) < 0) goto Error;
	
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

//------------------------------------------------------------------------------ 
// TaskTrig_type
//------------------------------------------------------------------------------ 
static TaskTrig_type* init_TaskTrig_type (Dev_type* dev, double* samplingRate)
{
	TaskTrig_type* taskTrig = malloc (sizeof(TaskTrig_type));
	if (!taskTrig) return NULL;
	
	taskTrig -> trigType				= Trig_None;
	taskTrig -> device					= dev;
	taskTrig -> trigSource				= NULL;
	taskTrig -> slope					= TrigSlope_Rising;
	taskTrig -> level					= 0;
	taskTrig -> wndBttm					= 0;
	taskTrig -> wndTop					= 0;
	taskTrig -> wndTrigCond				= TrigWnd_Entering;
	taskTrig -> nPreTrigSamples 		= 2;				// applies only to reference type trigger
	taskTrig -> trigSlopeCtrlID			= 0;	
	taskTrig -> trigSourceCtrlID		= 0;
	taskTrig -> preTrigDurationCtrlID	= 0;
	taskTrig -> preTrigNSamplesCtrlID	= 0;
	taskTrig -> levelCtrlID				= 0;
	taskTrig -> windowTrigCondCtrlID	= 0;
	taskTrig -> windowTopCtrlID			= 0;
	taskTrig -> windowBottomCtrlID		= 0;
	taskTrig -> samplingRate			= samplingRate;
		
	return taskTrig;	
}

static void	discard_TaskTrig_type (TaskTrig_type** taskTrigPtr)
{
	if (!*taskTrigPtr) return;
	OKfree((*taskTrigPtr)->trigSource);
	OKfree(*taskTrigPtr);
}

//------------------------------------------------------------------------------ 
// TaskTiming_type
//------------------------------------------------------------------------------ 
static TaskTiming_type*	init_TaskTiming_type (void)
{
	TaskTiming_type* taskTiming = malloc(sizeof(TaskTiming_type));
	if (!taskTiming) return NULL;
	
	taskTiming -> measMode					= DAQmxDefault_Task_MeasMode;
	taskTiming -> sampleRate				= DAQmxDefault_Task_SampleRate;
	taskTiming -> nSamples					= DAQmxDefault_Task_NSamples;
	taskTiming -> oversampling				= 1;
	taskTiming -> blockSize					= DAQmxDefault_Task_BlockSize;
	taskTiming -> sampClkSource				= NULL;   								// use onboard clock for sampling
	taskTiming -> sampClkEdge				= SampClockEdge_Rising;
	taskTiming -> refClkSource				= NULL;									// onboard clock has no external reference to sync to
	taskTiming -> refClkFreq				= DAQmxDefault_Task_RefClkFreq;
	taskTiming -> nSamplesSinkVChan			= NULL;
	taskTiming -> nSamplesSourceVChan		= NULL;
	taskTiming -> samplingRateSinkVChan		= NULL;
	taskTiming -> samplingRateSourceVChan	= NULL;
	
	// UI
	taskTiming -> timingPanHndl				= 0;
	taskTiming -> settingsPanHndl			= 0;
	
	return taskTiming;
}

static void discard_TaskTiming_type	(TaskTiming_type** taskTimingPtr)
{
	if (!*taskTimingPtr) return;
	
	OKfree((*taskTimingPtr)->sampClkSource);
	OKfree((*taskTimingPtr)->refClkSource);
	
	// VChans
	discard_VChan_type((VChan_type**)&(*taskTimingPtr)->nSamplesSinkVChan);
	discard_VChan_type((VChan_type**)&(*taskTimingPtr)->samplingRateSinkVChan);
	discard_VChan_type((VChan_type**)&(*taskTimingPtr)->nSamplesSourceVChan);
	discard_VChan_type((VChan_type**)&(*taskTimingPtr)->samplingRateSourceVChan);
	
	OKfree(*taskTimingPtr);
}

static CounterTaskTiming_type* init_CounterTaskTiming_type (void)
{
	CounterTaskTiming_type* taskTiming = malloc(sizeof(CounterTaskTiming_type));
	if (!taskTiming) return NULL;
	
	// Counter task
	taskTiming -> measMode				= MeasFinite;
	taskTiming -> nSamples				= DAQmxDefault_CO_Task_nPulses;
	taskTiming -> refNSamples			= &taskTiming->nSamples;
	taskTiming -> sampleRate			= DAQmxDefault_Task_SampleRate;
	taskTiming -> refSampleRate			= &taskTiming->sampleRate;
	taskTiming -> refClkFreq			= DAQmxDefault_Task_RefClkFreq;
	
	return taskTiming;
}

static void discard_CounterTaskTiming_type (TaskTiming_type** taskTimingPtr)
{
	if (!*taskTimingPtr) return;
	
	OKfree(*taskTimingPtr);
}

//------------------------------------------------------------------------------
// ADTaskSet_type
//------------------------------------------------------------------------------
static ADTaskSet_type* init_ADTaskSet_type (Dev_type* dev)
{
	ADTaskSet_type* taskSet = malloc (sizeof(ADTaskSet_type));
	if (!taskSet) return NULL;
	
			// init
			taskSet -> dev				= dev;
			taskSet -> chanSet			= 0;
			taskSet -> timing			= NULL;
			
			taskSet -> panHndl			= 0;
			taskSet -> taskHndl			= NULL;
			taskSet -> timeout			= DAQmxDefault_Task_Timeout;
	if (!(	taskSet -> chanSet			= ListCreate(sizeof(ChanSet_type*)))) 	goto Error;
	if (!(	taskSet -> timing			= init_TaskTiming_type()))				goto Error;	
			taskSet -> startTrig		= NULL;
			taskSet -> HWTrigMaster		= NULL;
			taskSet -> HWTrigSlave		= NULL;
			taskSet -> referenceTrig	= NULL;
			taskSet -> writeAOData		= NULL;
			taskSet -> readAIData		= NULL;
			taskSet -> writeDOData		= NULL;
		
	return taskSet;
Error:
	if (taskSet->chanSet) ListDispose(taskSet->chanSet);
	discard_TaskTiming_type(&taskSet->timing);
	OKfree(taskSet);
	return NULL;
}

static void	discard_ADTaskSet_type (ADTaskSet_type** taskSetPtr)
{
	if (!*taskSetPtr) return;
	
	// DAQmx task
	if ((*taskSetPtr)->taskHndl) {
		DAQmxTaskControl((*taskSetPtr)->taskHndl, DAQmx_Val_Task_Abort);
		DAQmxClearTask ((*taskSetPtr)->taskHndl); 
		(*taskSetPtr)->taskHndl = 0;
	}
	
	// channels
	if ((*taskSetPtr)->chanSet) {
		ChanSet_type** 		chanSetPtr;
		size_t				nChans			= ListNumItems((*taskSetPtr)->chanSet); 	
		for (size_t i = 1; i <= nChans; i++) {			
			chanSetPtr = ListGetPtrToItem((*taskSetPtr)->chanSet, i);
			(*(*chanSetPtr)->discardFptr)	((ChanSet_type**)chanSetPtr);
		}
		ListDispose((*taskSetPtr)->chanSet);
	}
	
	// trigger data
	discard_TaskTrig_type(&(*taskSetPtr)->startTrig);
	discard_TaskTrig_type(&(*taskSetPtr)->referenceTrig);
	discard_HWTrigMaster_type(&(*taskSetPtr)->HWTrigMaster);
	discard_HWTrigSlave_type(&(*taskSetPtr)->HWTrigSlave);
	
	// timing info
	discard_TaskTiming_type(&(*taskSetPtr)->timing);
	
	// AO streaming structure
	discard_WriteAOData_type(&(*taskSetPtr)->writeAOData);
	
	// AI read structure
	discard_ReadAIData_type(&(*taskSetPtr)->readAIData);
	
	// DO streaming structure
	discard_WriteDOData_type(&(*taskSetPtr)->writeDOData);
	
	OKfree(*taskSetPtr);
}

static void	newUI_ADTaskSet (ADTaskSet_type* tskSet, char taskSettingsTabName[], CVICtrlCBFptr_type removeDAQmxChanCBFptr, int taskTriggerUsage, 
							 char sinkVChanNSamplesBaseName[], char sourceVChanNSamplesBaseName[], char sinkVChanSamplingRateBaseName[], char sourceVChanSamplingRateBaseName[], char HWTrigBaseName[])
{
	//--------------------------
	// load UI resources
	//--------------------------
	int ADTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, ADTskSet); 
	
	// insert panel to UI and keep track of the task settings panel handle
	int newTabIdx = InsertPanelAsTabPage(tskSet->dev->devPanHndl, TaskSetPan_DAQTasks, -1, ADTaskSetPanHndl);
	DiscardPanel(ADTaskSetPanHndl);
	ADTaskSetPanHndl = 0;
	GetPanelHandleFromTabPage(tskSet->dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, &tskSet->panHndl);
	
	// change tab title to new Task Controller name
	SetTabPageAttribute(tskSet->dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, ATTR_LABEL_TEXT, taskSettingsTabName);
					
	// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
	int 	panHndl;
	void*   callbackData;
	GetPanelHandleFromTabPage(tskSet->dev->devPanHndl, TaskSetPan_DAQTasks, 0, &panHndl);
	GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
	if (!callbackData) DeleteTabPage(tskSet->dev->devPanHndl, TaskSetPan_DAQTasks, 0, 1);
	
	// connect task settings data to the panel
	SetPanelAttribute(tskSet->panHndl, ATTR_CALLBACK_DATA, tskSet);
								
	//--------------------------
	// adjust "Channels" tab
	//--------------------------
	// get channels tab panel handle
	int chanPanHndl;
	GetPanelHandleFromTabPage(tskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_ChanTabIdx, &chanPanHndl);
	
	// remove "None" labelled channel tab
	DeleteTabPage(chanPanHndl, Chan_ChanSet, 0, 1);
	
	// add callback data and callback function to remove channels
	SetCtrlAttribute(chanPanHndl, Chan_ChanSet, ATTR_CALLBACK_FUNCTION_POINTER, removeDAQmxChanCBFptr);
	SetCtrlAttribute(chanPanHndl, Chan_ChanSet, ATTR_CALLBACK_DATA, tskSet->dev);
								
	//--------------------------
	// adjust "Settings" tab
	//--------------------------
	// get settings tab panel handle
	GetPanelHandleFromTabPage(tskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_SettingsTabIdx, &tskSet->timing->settingsPanHndl);
	
	// set sampling rate
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_SamplingRate, tskSet->timing->sampleRate / 1000);					// display in [kHz]
	
	// set number of samples
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DATA_TYPE, VAL_UNSIGNED_64BIT_INTEGER);
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_NSamples, tskSet->timing->nSamples);
	
	// set block size
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_BlockSize, tskSet->timing->blockSize);
	
	// set duration
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate); 	// display in [s]
	
	// set oversampling if AI task
	if (tskSet->dev->AITaskSet == tskSet) {
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Oversampling, tskSet->timing->oversampling);					// set oversampling factor
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Oversampling, ATTR_VISIBLE, TRUE);
	}
	
	// set measurement mode
	InsertListItem(tskSet->timing->settingsPanHndl, Set_MeasMode, -1, "Finite", MeasFinite);
	InsertListItem(tskSet->timing->settingsPanHndl, Set_MeasMode, -1, "Continuous", MeasCont);
	int ctrlIdx;    
	GetIndexFromValue(tskSet->timing->settingsPanHndl, Set_MeasMode, &ctrlIdx, tskSet->timing->measMode);
	SetCtrlIndex(tskSet->timing->settingsPanHndl, Set_MeasMode, ctrlIdx);
	
	// add callback to controls in the panel
	SetCtrlsInPanCBInfo(tskSet, ADTaskSettings_CB, tskSet->timing->settingsPanHndl);
								
	//--------------------------
	// adjust "Timing" tab
	//--------------------------
	// get timing tab panel handle
	GetPanelHandleFromTabPage(tskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TimingTabIdx, &tskSet->timing->timingPanHndl);
	// make sure that the host controls are not dimmed before inserting terminal controls!
	NIDAQmx_NewTerminalCtrl(tskSet->timing->timingPanHndl, Timing_SampleClkSource, 0); // single terminal selection
	NIDAQmx_NewTerminalCtrl(tskSet->timing->timingPanHndl, Timing_RefClkSource, 0);    // single terminal selection
								
	// adjust sample clock terminal control properties
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
	// set default sample clock source
	tskSet->timing->sampClkSource = StrDup("OnboardClock");
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_SampleClkSource, "OnboardClock");
					
	// adjust reference clock terminal control properties
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0);
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
	// set default reference clock source (none)
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_RefClkSource, "");
					
	// set ref clock freq and timeout to default
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_RefClkFreq, tskSet->timing->refClkFreq / 1e6);		// display in [MHz]						
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_Timeout, tskSet->timeout);							// display in [s]
	// add callback to controls in the panel
	SetCtrlsInPanCBInfo(tskSet, ADTimingSettings_CB, tskSet->timing->timingPanHndl);
								
	//--------------------------
	// adjust "Trigger" tab
	//--------------------------
	// get trigger tab panel handle
	int trigPanHndl;
	GetPanelHandleFromTabPage(tskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TriggerTabIdx, &trigPanHndl);
								
	// remove "None" tab if there are supported triggers
	if (taskTriggerUsage)
		DeleteTabPage(trigPanHndl, Trig_TrigSet, 0, 1);
							
	// add supported trigger panels
	// start trigger
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_Start) {
		// load resources
		int start_DigEdgeTrig_PanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig1);
		// add trigger data structure
		tskSet->startTrig = init_TaskTrig_type(tskSet->dev, &tskSet->timing->sampleRate);
		// add start trigger panel
		int newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Start");
		// get start trigger tab panel handle
		int startTrigPanHndl;
		GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &startTrigPanHndl);
		// add control to select trigger type and put background plate
		DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_Plate, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		int trigTypeCtrlID = DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_TrigType, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		// add callback data and callback function to the control
		SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, tskSet->startTrig);
		SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskStartTrigType_CB);
		// insert trigger type options
		InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "None", Trig_None); 
		if (tskSet->dev->attr->DigitalTriggering) {
			InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Digital Edge", Trig_DigitalEdge); 
			InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Digital Pattern", Trig_DigitalPattern);
		}
									
		if (tskSet->dev->attr->AnalogTriggering) {
			InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Analog Edge", Trig_AnalogEdge); 
			InsertListItem(startTrigPanHndl, trigTypeCtrlID, -1, "Analog Window", Trig_AnalogWindow);
		}
		// set no trigger type by default
		SetCtrlIndex(startTrigPanHndl, trigTypeCtrlID, 0); 
									
		// discard resources
		DiscardPanel(start_DigEdgeTrig_PanHndl);
	}
								
	// reference trigger
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_Reference) {
		// load resources
		int reference_DigEdgeTrig_PanHndl	= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig1);
		// add trigger data structure
		tskSet->referenceTrig = init_TaskTrig_type(tskSet->dev, &tskSet->timing->sampleRate);
		// add reference trigger panel
		int newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Reference");
		// get reference trigger tab panel handle
		int referenceTrigPanHndl;
		GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &referenceTrigPanHndl);
		// add control to select trigger type
		DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_Plate, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		int trigTypeCtrlID = DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_TrigType, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		// add callback data and callback function to the control
		SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, tskSet->referenceTrig);
		SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskReferenceTrigType_CB);
		// insert trigger type options
		InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "None", Trig_None); 
		if (tskSet->dev->attr->DigitalTriggering) {
			InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Digital Edge", Trig_DigitalEdge); 
			InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Digital Pattern", Trig_DigitalPattern);
		}
					
		if (tskSet->dev->attr->AnalogTriggering) {
			InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Analog Edge", Trig_AnalogEdge); 
			InsertListItem(referenceTrigPanHndl, trigTypeCtrlID, -1, "Analog Window", Trig_AnalogWindow);
		}
		// set no trigger type by default
		SetCtrlIndex(referenceTrigPanHndl, trigTypeCtrlID, 0); 
							
		// discard resources
		DiscardPanel(reference_DigEdgeTrig_PanHndl);
	}
								
	// pause trigger
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_Pause) {
	}
								
	// advance trigger
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_Advance) {
	}
								
	// arm start trigger
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_ArmStart) {
	}
								
	// handshake trigger
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_Handshake) {
	}
						
	//-------------------------------------------------------------------------
	// add nSamples Sink VChan to pick-up number of required samples
	//-------------------------------------------------------------------------
						
	// create VChan
	char*	nSamplesSinkVChanName = GetTaskControlName(tskSet->dev->taskController);
	AppendString(&nSamplesSinkVChanName, ": ", -1);
	AppendString(&nSamplesSinkVChanName, sinkVChanNSamplesBaseName, -1);
						
	DLDataTypes		nSamplesVChanAllowedDataTypes[] = {DL_UChar, DL_UShort, DL_UInt, DL_ULong, DL_ULongLong};
						
	tskSet->timing->nSamplesSinkVChan	= init_SinkVChan_type(nSamplesSinkVChanName, nSamplesVChanAllowedDataTypes, NumElem(nSamplesVChanAllowedDataTypes),
															tskSet, VChan_Data_Receive_Timeout, ADNSamplesSinkVChan_Connected, ADNSamplesSinkVChan_Disconnected);
	OKfree(nSamplesSinkVChanName);
	// register VChan with the framework
	DLRegisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->nSamplesSinkVChan);
	// register VChan with the DAQmx task controller
	AddSinkVChan(tskSet->dev->taskController, tskSet->timing->nSamplesSinkVChan, ADNSamples_DataReceivedTC);
						
	//-------------------------------------------------------------------------
	// add nSamples Source VChan to send number of required samples
	//-------------------------------------------------------------------------
						
	// create VChan
	char*	nSamplesSourceVChanName = GetTaskControlName(tskSet->dev->taskController);
	AppendString(&nSamplesSourceVChanName, ": ", -1);
	AppendString(&nSamplesSourceVChanName, sourceVChanNSamplesBaseName, -1);
						
	tskSet->timing->nSamplesSourceVChan	= init_SourceVChan_type(nSamplesSourceVChanName, DL_ULongLong, tskSet, ADNSamplesSourceVChan_Connected, NULL);
	OKfree(nSamplesSourceVChanName);
	// register VChan with the framework
	DLRegisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->nSamplesSourceVChan);
						
	//-------------------------------------------------------------------------
	// add samplingRate Sink VChan to pick-up sampling rate
	//-------------------------------------------------------------------------
						
	// create VChan
	char*	samplingRateSinkVChanName = GetTaskControlName(tskSet->dev->taskController);
	AppendString(&samplingRateSinkVChanName, ": ", -1);
	AppendString(&samplingRateSinkVChanName, sinkVChanSamplingRateBaseName, -1);
						
	DLDataTypes		samplingRateVChanAllowedDataTypes[] = {DL_Double, DL_Float};
						
	tskSet->timing->samplingRateSinkVChan	= init_SinkVChan_type(samplingRateSinkVChanName, samplingRateVChanAllowedDataTypes, NumElem(samplingRateVChanAllowedDataTypes),
																	tskSet, VChan_Data_Receive_Timeout, ADSamplingRateSinkVChan_Connected, ADSamplingRateSinkVChan_Disconnected);
	OKfree(samplingRateSinkVChanName);
	// register VChan with the framework
	DLRegisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->samplingRateSinkVChan);
	// register VChan with the DAQmx task controller
	AddSinkVChan(tskSet->dev->taskController, tskSet->timing->samplingRateSinkVChan, ADSamplingRate_DataReceivedTC);
						
	//-------------------------------------------------------------------------
	// add samplingRate Source VChan to pick-up sampling rate
	//-------------------------------------------------------------------------
						
	// create VChan
	char*	samplingRateSourceVChanName = GetTaskControlName(tskSet->dev->taskController);
	AppendString(&samplingRateSourceVChanName, ": ", -1);
	AppendString(&samplingRateSourceVChanName, sourceVChanSamplingRateBaseName, -1);
						
	tskSet->timing->samplingRateSourceVChan	= init_SourceVChan_type(samplingRateSourceVChanName, DL_Double, tskSet, ADSamplingRateSourceVChan_Connected, NULL);
	OKfree(samplingRateSourceVChanName);
	// register VChan with the framework
	DLRegisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->samplingRateSourceVChan);
						
	//-------------------------------------------------------------------------
	// add HW Triggers
	//-------------------------------------------------------------------------
						
	// Master & Slave HW Triggers
	char*	HWTrigName = GetTaskControlName(tskSet->dev->taskController);
	AppendString(&HWTrigName, ": ", -1);
	AppendString(&HWTrigName, HWTrigBaseName, -1);
	tskSet->HWTrigMaster 	= init_HWTrigMaster_type(HWTrigName);
	tskSet->HWTrigSlave		= init_HWTrigSlave_type(HWTrigName);
	OKfree(HWTrigName);
						
	// register HW Triggers with framework
	DLRegisterHWTrigMaster(tskSet->HWTrigMaster);
	DLRegisterHWTrigSlave(tskSet->HWTrigSlave);
}

static void	discardUI_ADTaskSet (ADTaskSet_type** tskSetPtr)
{
	ADTaskSet_type* tskSet = *tskSetPtr;
	//------------------------------------------
	// remove nSamples Sink & Source VChans
	//------------------------------------------
				
	// remove from framework
	DLUnregisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->nSamplesSinkVChan);
	DLUnregisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->nSamplesSourceVChan);
	// detach from Task Controller										 
	RemoveSinkVChan(tskSet->dev->taskController, tskSet->timing->nSamplesSinkVChan);
				
	//------------------------------------------
	// remove Sink & Source sampling rate VChan
	//------------------------------------------
				
	// remove from framework
	DLUnregisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->samplingRateSinkVChan);
	DLUnregisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->samplingRateSourceVChan);
	// detach from Task Controller										 
	RemoveSinkVChan(tskSet->dev->taskController, tskSet->timing->samplingRateSinkVChan);
				
	//------------------------------------------
	// remove HW triggers from framework
	//------------------------------------------
				
	DLUnregisterHWTrigMaster(tskSet->HWTrigMaster);
	DLUnregisterHWTrigSlave(tskSet->HWTrigSlave);
			
	//------------------------------------------ 
	// discard AIAO task settings and VChan data
	//------------------------------------------
				
	discard_ADTaskSet_type(tskSetPtr);
}

// UI callback to adjust AI task settings
static int ADTaskSettings_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ADTaskSet_type*		tskSet 				= callbackData;
	char*				errMsg				= NULL;
	int					error				= 0;
	uInt64*				nSamplesPtr			= NULL;
	double*				samplingRatePtr		= NULL;
	DataPacket_type*	dataPacket			= NULL; 

	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case Set_SamplingRate:
			
			GetCtrlVal(panel, control, &tskSet->timing->sampleRate);   // read in [kHz]
			tskSet->timing->sampleRate *= 1000; 					   // transform to [Hz]
			// adjust duration display
			SetCtrlVal(panel, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate);
			
			// send sampling rate
			nullChk( samplingRatePtr = malloc(sizeof(double)) );
			*samplingRatePtr = tskSet->timing->sampleRate;
			nullChk( dataPacket = init_DataPacket_type(DL_Double, &samplingRatePtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errMsg) );
			
			break;
			
		case Set_NSamples:
			
			GetCtrlVal(panel, control, &tskSet->timing->nSamples);
			// adjust duration display
			SetCtrlVal(panel, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate);
			
			// send number of samples
			nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
			*nSamplesPtr = tskSet->timing->nSamples;
			nullChk( dataPacket = init_DataPacket_type(DL_ULongLong, &nSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errMsg) );
			
			break;
			
		case Set_Duration:
			
			double	duration;
			GetCtrlVal(panel, control, &duration);
			
			// calculate number of samples
			tskSet->timing->nSamples = (uInt64)(tskSet->timing->sampleRate * duration);
			if (tskSet->timing->nSamples < 2) tskSet->timing->nSamples = 2; // minimum allowed by DAQmx 
			// update display of number of samples & duration
			SetCtrlVal(panel, Set_NSamples, tskSet->timing->nSamples); 
			SetCtrlVal(panel, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate); 
			
			// send number of samples
			nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
			*nSamplesPtr = tskSet->timing->nSamples;
			nullChk( dataPacket = init_DataPacket_type(DL_ULongLong, &nSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errMsg) );
			
			break;
			
		case Set_Oversampling:
			
			GetCtrlVal(panel, control, &tskSet->timing->oversampling);
			
			discard_ReadAIData_type(&tskSet->readAIData);
			tskSet->readAIData = init_ReadAIData_type(tskSet->dev);
			
			break;
			
		case Set_BlockSize:
			
			GetCtrlVal(panel, control, &tskSet->timing->blockSize);
			break;
			
		case Set_MeasMode:
			
			unsigned int measMode;
			GetCtrlVal(panel, control, &measMode);
			tskSet->timing->measMode = measMode;
			
			switch (tskSet->timing->measMode) {
				case MeasFinite:
					SetCtrlAttribute(panel, Set_Duration, ATTR_DIMMED, 0);
					SetCtrlAttribute(panel, Set_NSamples, ATTR_DIMMED, 0); 
					break;
					
				case MeasCont:
					SetCtrlAttribute(panel, Set_Duration, ATTR_DIMMED, 1);
					SetCtrlAttribute(panel, Set_NSamples, ATTR_DIMMED, 1); 
					break;
			}
			break;
	}
	
	// update device settings
	if (tskSet->dev->AITaskSet == tskSet)
		errChk ( ConfigDAQmxAITask(tskSet->dev, &errMsg) );
	
	if (tskSet->dev->AOTaskSet == tskSet)
		errChk ( ConfigDAQmxAOTask(tskSet->dev, &errMsg) );
	
	if (tskSet->dev->DITaskSet == tskSet)
		errChk ( ConfigDAQmxDITask(tskSet->dev, &errMsg) );
	
	if (tskSet->dev->DOTaskSet == tskSet)
		errChk ( ConfigDAQmxDOTask(tskSet->dev, &errMsg) );
	
	return 0;
	
Error:
	
	// cleanup
	OKfree(nSamplesPtr);
	OKfree(samplingRatePtr);
	discard_DataPacket_type(&dataPacket); 
	
	if (errMsg) {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	} else
		DLMsg("Out of memory", 1);
	
	return 0;
}

static int ADTimingSettings_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ADTaskSet_type*		tskSet 			= callbackData;  
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case Timing_SampleClkSource:
			
			{	int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(tskSet->timing->sampClkSource);
				tskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, tskSet->timing->sampClkSource);
			}
			break;
			
		case Timing_RefClkSource:
			
			{	int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(tskSet->timing->sampClkSource);
				tskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, tskSet->timing->sampClkSource);
			}
			break;
			
		case Timing_RefClkFreq:
			
			GetCtrlVal(panel, control, &tskSet->timing->refClkFreq);
			break;
			
		case Timing_Timeout:
			
			GetCtrlVal(panel, control, &tskSet->timeout);
			break;
			
	}
	
	// update device settings
	if (tskSet->dev->AITaskSet == tskSet)
		errChk ( ConfigDAQmxAITask(tskSet->dev, &errMsg) );
	
	if (tskSet->dev->AOTaskSet == tskSet)
		errChk ( ConfigDAQmxAOTask(tskSet->dev, &errMsg) );
	
	if (tskSet->dev->DITaskSet == tskSet)
		errChk ( ConfigDAQmxDITask(tskSet->dev, &errMsg) );
	
	if (tskSet->dev->DOTaskSet == tskSet)
		errChk ( ConfigDAQmxDOTask(tskSet->dev, &errMsg) );
	
	return 0;
	
Error:
	
	if (errMsg) {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	} else
		DLMsg("Out of memory", 1);
	
	return 0;
}

//------------------------------------------------------------------------------
// ReadAIData_type
//------------------------------------------------------------------------------
static ReadAIData_type* init_ReadAIData_type (Dev_type* dev)
{
	ReadAIData_type*	readAI 		= NULL;
	size_t				nAITotal	= ListNumItems(dev->AITaskSet->chanSet);
	ChanSet_type**		chanSetPtr	= NULL;
	size_t				nAI			= 0; 
	size_t				chIdx		= 0;
	
	// count number of AI channels using HW-timing
	for (size_t i = 1; i <= nAITotal; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) nAI++;
	}
	// return NULL if there are no channels using HW-timing
	if (!nAI) return NULL;
	
	// allocate memory for processing AI data
	readAI 		= malloc (sizeof(ReadAIData_type));
	if (!readAI) return NULL;
	
	// init
	readAI->nAI 			= nAI;
	readAI->nIntBuffElem 	= NULL;
	readAI->intBuffers 		= NULL;
	
	// alloc
	if ( !(readAI->nIntBuffElem = calloc(nAI, sizeof(uInt32))) ) goto Error;
	if ( !(readAI->intBuffers 	= calloc(nAI, sizeof(float64*))) ) goto Error;
	for (size_t i = 1; i <= nAITotal; i++) {
		chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		if ((*chanSetPtr)->onDemand) continue;
		
		if ( !(readAI->intBuffers[chIdx] = calloc(dev->AITaskSet->timing->oversampling + dev->AITaskSet->timing->blockSize, sizeof(float64))) ) goto Error;
		
		chIdx++;
	}
	
	return readAI;
	
Error:
	
	discard_ReadAIData_type(&readAI);
	return NULL;
}

static void discard_ReadAIData_type (ReadAIData_type** readAIPtr)
{
	if (!*readAIPtr) return;
	
	OKfree((*readAIPtr)->nIntBuffElem);
	
	if ((*readAIPtr)->intBuffers) {
		for (size_t i = 0; i < (*readAIPtr)->nAI; i++)
			OKfree((*readAIPtr)->intBuffers[i]);
		
		OKfree((*readAIPtr)->intBuffers);
	}
	
	OKfree(*readAIPtr);
}

//------------------------------------------------------------------------------
// WriteAOData_type
//------------------------------------------------------------------------------
static WriteAOData_type* init_WriteAOData_type (Dev_type* dev)
{
	ChanSet_type**		chanSetPtr;
	size_t				nAO				= 0;
	size_t				i;
	
	// count number of AO channels using HW-timing
	size_t	nChans	= ListNumItems(dev->AOTaskSet->chanSet); 	
	for (i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) nAO++;
	}
	
	WriteAOData_type* 	writeData				= malloc(sizeof(WriteAOData_type));
	if (!writeData) return NULL;
	
	
	
			writeData -> writeblock				= dev->AOTaskSet->timing->blockSize;
			writeData -> numchan				= nAO;
	
	// init
			writeData -> datain           		= NULL;
			writeData -> databuff         		= NULL;
			writeData -> dataout          		= NULL;
			writeData -> sinkVChans        		= NULL;
			writeData -> datain_size      		= NULL;
			writeData -> databuff_size    		= NULL;
			writeData -> idx              		= NULL;
			writeData -> datain_repeat    		= NULL;
			writeData -> datain_remainder 		= NULL;
			writeData -> datain_loop      		= NULL;
			writeData -> nullPacketReceived		= NULL;
			writeData -> writeBlocksLeftToWrite	= 2;
	
	// datain
	if (!(	writeData -> datain					= malloc(nAO * sizeof(float64*))) && nAO)							goto Error;
	for (i = 0; i < nAO; i++) writeData->datain[i] = NULL;
	
	// databuff
	if (!(	writeData -> databuff   			= malloc(nAO * sizeof(float64*))) && nAO)							goto Error;
	for (i = 0; i < nAO; i++) writeData->databuff[i] = NULL;
	
	// dataout
	if (!(	writeData -> dataout 				= malloc(nAO * writeData->writeblock * sizeof(float64))) && nAO)	goto Error;
	
	// sink VChans
	if (!(	writeData -> sinkVChans				= malloc(nAO * sizeof(SinkVChan_type*))) && nAO)					goto Error;
	
	nChans		= ListNumItems(dev->AOTaskSet->chanSet); 
	size_t k 	= 0;
	for (i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) {
			writeData->sinkVChans[k] = (*chanSetPtr)->sinkVChan;
			k++;
		}
	}
	
	// datain_size
	if (!(	writeData -> datain_size 			= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (i = 0; i < nAO; i++) writeData->datain_size[i] = 0;
		
	// databuff_size
	if (!(	writeData -> databuff_size 			= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (i = 0; i < nAO; i++) writeData->databuff_size[i] = 0;
		
	// idx
	if (!(	writeData -> idx 					= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (i = 0; i < nAO; i++) writeData->idx[i] = 0;
		
	// datain_repeat
	if (!(	writeData -> datain_repeat 			= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (i = 0; i < nAO; i++) writeData->datain_repeat[i] = 0;
		
	// datain_remainder
	if (!(	writeData -> datain_remainder 		= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (i = 0; i < nAO; i++) writeData->datain_remainder[i] = 0;
		
	// datain_loop
	if (!(	writeData -> datain_loop 			= malloc(nAO * sizeof(BOOL))) && nAO)								goto Error;
	for (i = 0; i < nAO; i++) writeData->datain_loop[i] = FALSE;
	
	// nullPacketReceived
	if (!(	writeData -> nullPacketReceived		= malloc(nAO * sizeof(BOOL))) && nAO)								goto Error;
	for (i = 0; i < nAO; i++) writeData->nullPacketReceived[i] = FALSE;
	
	return writeData;
	
Error:
	
	discard_WriteAOData_type(&writeData);
	
	return NULL;
}

static void	discard_WriteAOData_type (WriteAOData_type** writeDataPtr)
{
	if (!*writeDataPtr) return;
	
	for (size_t i = 0; i < (*writeDataPtr)->numchan; i++) {
		OKfree((*writeDataPtr)->datain[i]);	    
		OKfree((*writeDataPtr)->databuff[i]);   
	}
	
	OKfree((*writeDataPtr)->databuff);
	OKfree((*writeDataPtr)->datain); 
	OKfree((*writeDataPtr)->dataout);
	OKfree((*writeDataPtr)->sinkVChans);
	OKfree((*writeDataPtr)->datain_size);
	OKfree((*writeDataPtr)->databuff_size);
	OKfree((*writeDataPtr)->idx);
	OKfree((*writeDataPtr)->datain_repeat);
	OKfree((*writeDataPtr)->datain_remainder);
	OKfree((*writeDataPtr)->datain_loop);
	OKfree((*writeDataPtr)->nullPacketReceived)
	
	OKfree(*writeDataPtr);
}

//------------------------------------------------------------------------------
// WriteDOData_type
//------------------------------------------------------------------------------
static WriteDOData_type* init_WriteDOData_type (Dev_type* dev)
{
	ChanSet_type**		chanSetPtr;
	size_t				nDO				= 0;
	size_t				i;
	
	// count number of DO channels using HW-timing
	size_t	nChans	= ListNumItems(dev->DOTaskSet->chanSet); 	
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) nDO++;
	}
	// return NULL if there are no channels using HW-timing
	if (!nDO) return NULL;
	
	WriteDOData_type* 	writeData	= malloc(sizeof(WriteDOData_type));
	if (!writeData) return NULL;
	
	
	
			writeData -> writeblock			= dev->DOTaskSet->timing->blockSize;
			writeData -> numchan			= nDO;
	
	// init
			writeData -> datain           	= NULL;
			writeData -> databuff         	= NULL;
			writeData -> dataout          	= NULL;
			writeData -> sinkVChans        	= NULL;
			writeData -> datain_size      	= NULL;
			writeData -> databuff_size    	= NULL;
			writeData -> idx              	= NULL;
			writeData -> datain_repeat    	= NULL;
			writeData -> datain_remainder 	= NULL;
			writeData -> datain_loop      	= NULL;
	
	// datain
	if (!(	writeData -> datain				= malloc(nDO * sizeof(uInt32*)))) 							goto Error;
	for (i = 0; i < nDO; i++) writeData->datain[i] = NULL;
	
	// databuff
	if (!(	writeData -> databuff   		= malloc(nDO * sizeof(uInt32*)))) 							goto Error;
	for (i = 0; i < nDO; i++) writeData->databuff[i] = NULL;
	
	// dataout
	if (!(	writeData -> dataout 			= malloc(nDO * writeData->writeblock * sizeof(uInt32))))	goto Error;
	
	// sink VChans
	if (!(	writeData -> sinkVChans			= malloc(nDO * sizeof(SinkVChan_type*))))					goto Error;
	
	nChans		= ListNumItems(dev->DOTaskSet->chanSet); 
	size_t k 	= 0;
	for (i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) {
			writeData->sinkVChans[k] = (*chanSetPtr)->sinkVChan;
			k++;
		}
	}
	
	// datain_size
	if (!(	writeData -> datain_size 		= malloc(nDO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nDO; i++) writeData->datain_size[i] = 0;
		
	// databuff_size
	if (!(	writeData -> databuff_size 		= malloc(nDO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nDO; i++) writeData->databuff_size[i] = 0;
		
	// idx
	if (!(	writeData -> idx 				= malloc(nDO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nDO; i++) writeData->idx[i] = 0;
		
	// datain_repeat
	if (!(	writeData -> datain_repeat 		= malloc(nDO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nDO; i++) writeData->datain_repeat[i] = 0;
		
	// datain_remainder
	if (!(	writeData -> datain_remainder 	= malloc(nDO * sizeof(size_t))))							goto Error;
	for (i = 0; i < nDO; i++) writeData->datain_remainder[i] = 0;
		
	// datain_loop
	if (!(	writeData -> datain_loop 		= malloc(nDO * sizeof(BOOL))))								goto Error;
	for (i = 0; i < nDO; i++) writeData->datain_loop[i] = 0;
	
	return writeData;
	
Error:
	OKfree(writeData->datain);
	OKfree(writeData->databuff);
	OKfree(writeData->dataout);
	OKfree(writeData->sinkVChans);
	OKfree(writeData->datain_size);
	OKfree(writeData->databuff_size);
	OKfree(writeData->idx);
	OKfree(writeData->datain_repeat);
	OKfree(writeData->datain_remainder);
	OKfree(writeData->datain_loop);
	
	OKfree(writeData);
	return NULL;
}

static void	discard_WriteDOData_type (WriteDOData_type** writeDataPtr)
{
	if (!*writeDataPtr) return;
	
	for (size_t i = 0; i < (*writeDataPtr)->numchan; i++) {
		OKfree((*writeDataPtr)->datain[i]);
		OKfree((*writeDataPtr)->databuff[i]);
	}
	
	OKfree((*writeDataPtr)->dataout);
	OKfree((*writeDataPtr)->sinkVChans);
	OKfree((*writeDataPtr)->datain_size);
	OKfree((*writeDataPtr)->databuff_size);
	OKfree((*writeDataPtr)->idx);
	OKfree((*writeDataPtr)->datain_repeat);
	OKfree((*writeDataPtr)->datain_remainder);
	OKfree((*writeDataPtr)->datain_loop);
	
	OKfree(*writeDataPtr);
}

static int DO_Timing_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type*			dev 			= callbackData;
	char*				errMsg			= NULL;
	int					error			= 0;
	int 				intval;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) { 
			
		case Timing_Timeout:
			
			GetCtrlVal(panel, control, &dev->DOTaskSet->timeout);
			break;
			
		case Timing_SampleClkSource:
			
			int buffsize;
			GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
			OKfree(dev->DOTaskSet->timing->sampClkSource);
			dev->DOTaskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
			GetCtrlVal(panel, control, dev->DOTaskSet->timing->sampClkSource);
			break;
			
	}
	
	// configure DO
	errChk ( ConfigDAQmxDOTask(dev, &errMsg) );  
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
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
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems((*a)->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem((*a)->chanTaskSet, i);
			(*(*chanSetPtr)->discardFptr)	((ChanSet_type**)chanSetPtr);
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
	
	if (!(	a -> chanTaskSet	= ListCreate(sizeof(ChanSet_CO_type*)))) 	goto Error;
	
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
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems((*a)->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem((*a)->chanTaskSet, i);
			(*(*chanSetPtr)->discardFptr)	((ChanSet_type**)chanSetPtr);
		}
		ListDispose((*a)->chanTaskSet);
	}
	
	OKfree(*a);
	
}

static int CVICALLBACK Chan_CO_Pulse_Frequency_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* 	selectedChan 	= callbackData;
	DataPacket_type*  	dataPacket		= NULL;
	char*				errMsg			= NULL;
	int					error			= 0;
	int					generationMode;
	double 				frequency;
	double				dutyCycle;
	double				initialDelay;
	int 				idleState;
	uInt64				nPulses;
	PulseTrain_type* 	pulseTrain		= NULL;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
		
		case COFreqPan_Mode:
			GetCtrlVal(panel, control, &generationMode);
			if (generationMode == MeasFinite) {
				selectedChan->taskTiming->measMode = MeasFinite;
				SetPulseTrainMode(selectedChan->pulseTrain, MeasFinite);
				// undim N Pulses
				SetCtrlAttribute(panel, COFreqPan_NPulses, ATTR_DIMMED, 0); 
			} else {
				selectedChan->taskTiming->measMode = MeasCont;
				SetPulseTrainMode(selectedChan->pulseTrain, MeasCont);
				// dim N Pulses
				SetCtrlAttribute(panel, COFreqPan_NPulses, ATTR_DIMMED, 1); 
			}
			break;
			
		case COFreqPan_NPulses:
			GetCtrlVal(panel, control, &nPulses);
			SetPulseTrainNPulses(selectedChan->pulseTrain, nPulses);
			break;
			
		case COFreqPan_Frequency:
			GetCtrlVal(panel, control, &frequency);
			SetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)selectedChan->pulseTrain, frequency);
			break;
			
		case COFreqPan_DutyCycle:
			GetCtrlVal(panel, control, &dutyCycle); 
			SetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)selectedChan->pulseTrain, dutyCycle); 
			break;
			
		case COFreqPan_InitialDelay:
			GetCtrlVal(panel, control, &initialDelay);		// read in [ms]
			initialDelay *= 1e-3;   						// convert to [s] 
			SetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)selectedChan->pulseTrain, initialDelay);
			break;
			
		case COFreqPan_IdleState:
			GetCtrlVal(panel, control, &idleState);
			if (idleState == 0) 
				SetPulseTrainIdleState(selectedChan->pulseTrain, PulseTrainIdle_Low);
			else 
				SetPulseTrainIdleState(selectedChan->pulseTrain, PulseTrainIdle_High);
			break;
	}
	
	// configure CO task
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errMsg) );
	
	// send new pulse train
	nullChk( pulseTrain	= CopyPulseTrain(selectedChan->pulseTrain) ); 
	nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, &pulseTrain, NULL, (DiscardPacketDataFptr_type) discard_PulseTrain_type) );
	errChk( SendDataPacket(selectedChan->baseClass.srcVChan, &dataPacket, 0, &errMsg) ); 
	
	
	return 0;
	
Error:
	
	// clean up
	discard_DataPacket_type(&dataPacket);
	discard_PulseTrain_type(&pulseTrain);
	
	if (!errMsg)
		DLMsg("Chan_CO_Pulse_Frequency_CB Error: Out of memory", 1);	
	else {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	}
	
	return 0;
}

static int CVICALLBACK Chan_CO_Pulse_Time_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* 	selectedChan 	= callbackData;
	DataPacket_type*  	dataPacket		= NULL;
	char*				errMsg			= NULL;
	int					error			= 0;
	int					generationMode;
	double				time;
	double				initialDelay;
	int 				idleState;
	uInt64				nPulses;
	PulseTrain_type* 	pulseTrain		= NULL;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case COTimePan_Mode:
			GetCtrlVal(panel, control, &generationMode);
			if (generationMode == MeasFinite) {
				selectedChan->taskTiming->measMode = MeasFinite;
				SetPulseTrainMode(selectedChan->pulseTrain, MeasFinite);
				// undim N Pulses
				SetCtrlAttribute(panel, COTimePan_NPulses, ATTR_DIMMED, 0); 
			} else {
				selectedChan->taskTiming->measMode = MeasCont;
				SetPulseTrainMode(selectedChan->pulseTrain, MeasCont);
				// dim N Pulses
				SetCtrlAttribute(panel, COTimePan_NPulses, ATTR_DIMMED, 1); 
			}
			break;
			
		case COTimePan_NPulses:
			GetCtrlVal(panel, control, &nPulses);
			SetPulseTrainNPulses(selectedChan->pulseTrain, nPulses);
			break;
		
		case COTimePan_LowTime:
			GetCtrlVal(panel, control, &time);				// read in [ms]
			time *= 1e-3;									// convert to [s]
			SetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)selectedChan->pulseTrain, time); 
			break;
			
		case COTimePan_HighTime:
			GetCtrlVal(panel, control, &time);				// read in [ms]
			time *= 1e-3;									// convert to [s] 
			SetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)selectedChan->pulseTrain, time); 
			break;
			
		case COTimePan_InitialDelay:
			GetCtrlVal(panel, control, &initialDelay);		// read in [ms]
			initialDelay *= 1e-3;   						// convert to [s] 
			SetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)selectedChan->pulseTrain, initialDelay);
			break;
			
		case COTimePan_IdleState:
			GetCtrlVal(panel, control, &idleState);
			if (idleState == 0) 
				SetPulseTrainIdleState(selectedChan->pulseTrain, PulseTrainIdle_Low);
			else 
				SetPulseTrainIdleState(selectedChan->pulseTrain, PulseTrainIdle_High);
			break;
	}
	
	// configure CO task
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errMsg) );
	
	// send new pulse train
	nullChk( pulseTrain	= CopyPulseTrain(selectedChan->pulseTrain) ); 
	nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, &pulseTrain, NULL, (DiscardPacketDataFptr_type) discard_PulseTrain_type) );
	errChk( SendDataPacket(selectedChan->baseClass.srcVChan, &dataPacket, 0, &errMsg) ); 
	
	return 0;
	
Error:
	
	// clean up
	discard_DataPacket_type(&dataPacket);
	discard_PulseTrain_type(&pulseTrain);
	
	if (!errMsg)
		DLMsg("Chan_CO_Pulse_Frequency_CB Error: Out of memory", 1);	
	else {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	}
	
	return 0;
}

static int CVICALLBACK Chan_CO_Pulse_Ticks_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* 	selectedChan 	= callbackData;
	DataPacket_type*  	dataPacket		= NULL;
	char*				errMsg			= NULL;
	int					error			= 0;
	int					generationMode;
	int					ticks;
	int 				idleState;
	uInt64				nPulses;
	PulseTrain_type* 	pulseTrain		= NULL;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case COTicksPan_Mode:
			GetCtrlVal(panel, control, &generationMode);
			if (generationMode == MeasFinite) {
				selectedChan->taskTiming->measMode = MeasFinite;
				SetPulseTrainMode(selectedChan->pulseTrain, MeasFinite);
				// undim N Pulses
				SetCtrlAttribute(panel, COTicksPan_NPulses, ATTR_DIMMED, 0); 
			} else {
				selectedChan->taskTiming->measMode = MeasCont;
				SetPulseTrainMode(selectedChan->pulseTrain, MeasCont); 
				// dim N Pulses
				SetCtrlAttribute(panel, COTicksPan_NPulses, ATTR_DIMMED, 1); 
			}
			break;
			
		case COTicksPan_NPulses:
			GetCtrlVal(panel, control, &nPulses);
			SetPulseTrainNPulses(selectedChan->pulseTrain, nPulses);
			break;
		
		case COTicksPan_LowTicks:
			GetCtrlVal(panel, control, &ticks);
			SetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)selectedChan->pulseTrain, ticks); 
			break;
			
		case COTicksPan_HighTicks:
			GetCtrlVal(panel, control, &ticks);
			SetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)selectedChan->pulseTrain, ticks); 
			break;
			
		case COTicksPan_InitialDelay:
			GetCtrlVal(panel, control, &ticks);
			SetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)selectedChan->pulseTrain, ticks); 
			break;
			
		case COTicksPan_IdleState:
			GetCtrlVal(panel, control, &idleState);
			if (idleState == 0) 
				SetPulseTrainIdleState(selectedChan->pulseTrain, PulseTrainIdle_Low);
			else 
				SetPulseTrainIdleState(selectedChan->pulseTrain, PulseTrainIdle_High);
			break;
	}
	
	// configure CO task
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errMsg) );
	
	// send new pulse train
	nullChk( pulseTrain	= CopyPulseTrain(selectedChan->pulseTrain) ); 
	nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, &pulseTrain, NULL, (DiscardPacketDataFptr_type) discard_PulseTrain_type) );
	errChk( SendDataPacket(selectedChan->baseClass.srcVChan, &dataPacket, 0, &errMsg) ); 
	
	return 0;
	
Error:
	
	// clean up
	discard_DataPacket_type(&dataPacket);
	discard_PulseTrain_type(&pulseTrain);
	
	if (!errMsg)
		DLMsg("Chan_CO_Pulse_Frequency_CB Error: Out of memory", 1);	
	else {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	}
	
	return 0;
}

static int CVICALLBACK Chan_CO_Clk_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* 	selectedChan 	= callbackData; 
	char*				errMsg			= NULL;
	int					error			= 0;
	int 				buffsize 		= 0; 
	int 				intval;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) { 
			
		case CLKPAN_RefClockSlope:
			
			GetCtrlVal(panel, control, &intval);
			if (intval == TrigSlope_Rising) selectedChan->baseClass.referenceTrig->slope=TrigSlope_Rising;
			if (intval == TrigSlope_Falling) selectedChan->baseClass.referenceTrig->slope=TrigSlope_Falling; 
			break;
			
 		case CLKPAN_RefClockType:
			
			GetCtrlVal(panel, control, &intval); 
			if (intval == Trig_None) {
				selectedChan->baseClass.referenceTrig->trigType = Trig_None;
				
				SetCtrlAttribute(panel,CLKPAN_RefClockSlope,ATTR_DIMMED,TRUE);
				SetCtrlAttribute(panel,CLKPAN_RefClkSource,ATTR_DIMMED,TRUE); 
			}
			
			if (intval == Trig_DigitalEdge) {
				selectedChan->baseClass.referenceTrig->trigType = Trig_DigitalEdge;
				
				SetCtrlAttribute(panel,CLKPAN_RefClockSlope,ATTR_DIMMED,FALSE);
				SetCtrlAttribute(panel,CLKPAN_RefClkSource,ATTR_DIMMED,FALSE); 
			}
			break;
			
		case CLKPAN_RefClkSource:
			
			OKfree(selectedChan->baseClass.referenceTrig->trigSource);
			GetCtrlValStringLength(panel, control, &buffsize);
			selectedChan->baseClass.referenceTrig->trigSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
			GetCtrlVal(panel, control, selectedChan->baseClass.referenceTrig->trigSource);
			break;
			
	}
	
	// configure CO task
	errChk(ConfigDAQmxCOTask(selectedChan->baseClass.device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK Chan_CO_Trig_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CO_type* 			selectedChan 		= callbackData; 
	char*						errMsg				= NULL;
	int							error				= 0;
	int 						buffsize 			= 0; 
	int 						intval;
	
	
	if (event != EVENT_COMMIT) return 0;
	
	
	switch (control) { 							  
			
		case TrigPan_Slope:
			
		  	GetCtrlVal(panel, control, &intval);
			if (intval == TrigSlope_Rising) selectedChan->baseClass.startTrig->slope = TrigSlope_Rising;
			if (intval == TrigSlope_Falling) selectedChan->baseClass.startTrig->slope = TrigSlope_Falling; 
			break;
			
 		case TrigPan_TrigType:
			
			GetCtrlVal(panel, control,&intval); 
			if (intval == Trig_None) {
				selectedChan->baseClass.startTrig->trigType = DAQmx_Val_None;
				SetCtrlAttribute(panel,TrigPan_Slope,ATTR_DIMMED,TRUE);
				SetCtrlAttribute(panel,TrigPan_Source,ATTR_DIMMED,TRUE); 
			}
			
			if (intval == Trig_DigitalEdge) {
				selectedChan->baseClass.startTrig->trigType = DAQmx_Val_DigEdge;
				SetCtrlAttribute(panel,TrigPan_Slope,ATTR_DIMMED,FALSE);
				SetCtrlAttribute(panel,TrigPan_Source,ATTR_DIMMED,FALSE); 
			}
			break;
			
		case TrigPan_Source:
			
			OKfree(selectedChan->baseClass.startTrig->trigSource);
			GetCtrlValStringLength(panel, control, &buffsize);
			selectedChan->baseClass.startTrig->trigSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
			GetCtrlVal(panel, TrigPan_Source, selectedChan->baseClass.startTrig->trigSource);
			break;
			
	}
	
	// configure CO task
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
	
}

static int DAQmxDevTaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Dev_type* 		dev	 		= (Dev_type*) callbackData;
	int				ioVal;
	int				ioMode;
	int				ioType;
	char*			errMsg		= NULL;
	int				error		= 0;
	
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
					errChk( AddDAQmxChannel(dev, ioVal, ioMode, ioType, chanName, &errMsg) );
					
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
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

//---------------------------------------------------------------------------------------------------------------
// DAQmx Device Task Configuration
//---------------------------------------------------------------------------------------------------------------

static int ConfigDAQmxDevice (Dev_type* dev, char** errorInfo)
{
	int		error		= 0;
	
	// Configure AI DAQmx Task
	errChk( ConfigDAQmxAITask(dev, errorInfo) );
	
	// Configure AO DAQmx Task
	errChk( ConfigDAQmxAOTask(dev, errorInfo) );
	
	// Configure DI DAQmx Task
	errChk( ConfigDAQmxDITask(dev, errorInfo) );
	
	// Configure DO DAQmx Task
	errChk( ConfigDAQmxDOTask(dev, errorInfo) );
	
	// Configure CI DAQmx Task
	errChk( ConfigDAQmxCITask(dev, errorInfo) );
	
	// Configure CO DAQmx Task
	errChk( ConfigDAQmxCOTask(dev, errorInfo) );
	
	return 0;
	
Error:
	
	ClearDAQmxTasks(dev);
	return error;
}

static int ConfigDAQmxAITask (Dev_type* dev, char** errorInfo)
{
#define ConfigDAQmxAITask_Err_ChannelNotImplemented		-1
	int 				error 			= 0;
	char*				errMsg			= NULL;
	ChanSet_type** 		chanSetPtr;
	
	if (!dev->AITaskSet) return 0; 		// do nothing
	
	// clear AI task if any
	if (dev->AITaskSet->taskHndl) {
		DAQmxErrChk(DAQmxClearTask(dev->AITaskSet->taskHndl));
		dev->AITaskSet->taskHndl = NULL;
	}
	
	// clear and init readAIData used for processing incoming data
	discard_ReadAIData_type(&dev->AITaskSet->readAIData);
	nullChk( dev->AITaskSet->readAIData = init_ReadAIData_type(dev) );
	
	// check if there is at least one AI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nChans 			= ListNumItems(dev->AITaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the AI task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create DAQmx AI task 
	char* taskName = GetTaskControlName(dev->taskController);
	AppendString(&taskName, " AI Task", -1);
	DAQmxErrChk (DAQmxCreateTask(taskName, &dev->AITaskSet->taskHndl));
	OKfree(taskName);
	
	// create AI channels
	nChans = ListNumItems(dev->AITaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		
		switch((*chanSetPtr)->chanType) {
			
			case Chan_AI_Voltage:
				
				DAQmxErrChk (DAQmxCreateAIVoltageChan(dev->AITaskSet->taskHndl, (*chanSetPtr)->name, "", (*(ChanSet_AIAO_Voltage_type**)chanSetPtr)->terminal, 
													  (*(ChanSet_AIAO_Voltage_type**)chanSetPtr)->Vmin, (*(ChanSet_AIAO_Voltage_type**)chanSetPtr)->Vmax, DAQmx_Val_Volts, NULL)); 
				
				break;
				
			case Chan_AI_Current:
				
				DAQmxErrChk (DAQmxCreateAICurrentChan(dev->AITaskSet->taskHndl, (*chanSetPtr)->name, "", (*(ChanSet_AIAO_Current_type**)chanSetPtr)->terminal, 
													  (*(ChanSet_AIAO_Current_type**)chanSetPtr)->Imin, (*(ChanSet_AIAO_Current_type**)chanSetPtr)->Imax, DAQmx_Val_Amps,
													  (*(ChanSet_AIAO_Current_type**)chanSetPtr)->shuntResistorType, (*(ChanSet_AIAO_Current_type**)chanSetPtr)->shuntResistorValue, NULL));
							  
				break;
				
			// add here more channel type cases
			
			default:
				if (errorInfo)
					*errorInfo = FormatMsg(ConfigDAQmxAITask_Err_ChannelNotImplemented, "ConfigDAQmxAITask", "Channel type not implemented");
				return ConfigDAQmxAITask_Err_ChannelNotImplemented; 
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
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_Rate, dev->AITaskSet->timing->sampleRate * dev->AITaskSet->timing->oversampling));
	
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
		uInt64		quot;
		uInt32		defaultBuffSize; 
		
		case MeasFinite:
			
			// set number of samples per channel
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->AITaskSet->timing->nSamples * dev->AITaskSet->timing->oversampling));
			quot = (dev->AITaskSet->timing->nSamples * dev->AITaskSet->timing->oversampling) / dev->AITaskSet->timing->blockSize; 
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
				DAQmxErrChk (DAQmxDisableStartTrig(dev->AITaskSet->taskHndl));
				break;
			
			case Trig_AnalogEdge:
				if (dev->AITaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->slope, dev->AITaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AITaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->wndTrigCond, 
							 dev->AITaskSet->startTrig->wndTop, dev->AITaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AITaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	// configure reference trigger
	if (dev->AITaskSet->referenceTrig)
		switch (dev->AITaskSet->referenceTrig->trigType) {
			
			case Trig_None:
				DAQmxErrChk (DAQmxDisableRefTrig(dev->AITaskSet->taskHndl));
				break;
			
			case Trig_AnalogEdge:
				if (dev->AITaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->slope, dev->AITaskSet->referenceTrig->level, dev->AITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AITaskSet->referenceTrig->trigSource)
				DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->wndTrigCond, 
							 dev->AITaskSet->referenceTrig->wndTop, dev->AITaskSet->referenceTrig->wndBttm, dev->AITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AITaskSet->referenceTrig->trigSource)
				DAQmxErrChk (DAQmxCfgDigEdgeRefTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->slope, dev->AITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	//----------------------
	// Add AI Task callbacks
	//----------------------
	// register AI data available callback 
	DAQmxErrChk( DAQmxRegisterEveryNSamplesEvent(dev->AITaskSet->taskHndl, DAQmx_Val_Acquired_Into_Buffer, dev->AITaskSet->timing->blockSize, 0, AIDAQmxTaskDataAvailable_CB, dev) ); 
	// register AI task done event callback
	// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
	// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
	DAQmxErrChk( DAQmxRegisterDoneEvent(dev->AITaskSet->taskHndl, 0, AIDAQmxTaskDone_CB, dev) );
	
	//----------------------  
	// Commit AI Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;
	
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	nullChk(errMsg = malloc((buffsize+1)*sizeof(char)));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ConfigDAQmxAITask", errMsg);
	OKfree(errMsg);
	return error;
}

static int ConfigDAQmxAOTask (Dev_type* dev, char** errorInfo)
{
#define ConfigDAQmxAOTask_Err_OutOfMemory				-1
#define ConfigDAQmxAOTask_Err_ChannelNotImplemented		-2
	
	int 				error 			= 0;
	char*				errMsg			= NULL;
	ChanSet_type** 		chanSetPtr;
	
	if (!dev->AOTaskSet) return 0; 		// do nothing
	
	// clear AO task if any
	if (dev->AOTaskSet->taskHndl) {
		DAQmxErrChk(DAQmxClearTask(dev->AOTaskSet->taskHndl));
		dev->AOTaskSet->taskHndl = NULL;
	}
	
	// clear and init writeAOData used for continuous streaming
	discard_WriteAOData_type(&dev->AOTaskSet->writeAOData);
	nullChk( dev->AOTaskSet->writeAOData = init_WriteAOData_type(dev) );
	
	// check if there is at least one AO task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nChans			= ListNumItems(dev->AOTaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) {
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
	nChans			= ListNumItems(dev->AOTaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
	
		// include in the task only channels for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		
		switch((*chanSetPtr)->chanType) {
			
			case Chan_AO_Voltage:
				
				// create channel
				DAQmxErrChk (DAQmxCreateAOVoltageChan(dev->AOTaskSet->taskHndl, (*chanSetPtr)->name, "", (*(ChanSet_AIAO_Voltage_type**)chanSetPtr)->Vmin, 
													  (*(ChanSet_AIAO_Voltage_type**)chanSetPtr)->Vmax, DAQmx_Val_Volts, NULL)); 
				
				break;
				
			case Chan_AO_Current:
				
				DAQmxErrChk (DAQmxCreateAOCurrentChan(dev->AOTaskSet->taskHndl, (*chanSetPtr)->name, "", (*(ChanSet_AIAO_Current_type**)chanSetPtr)->Imin,
													  (*(ChanSet_AIAO_Current_type**)chanSetPtr)->Imax, DAQmx_Val_Amps, NULL));
							  
				break;
			/*	
			case Chan_AO_Function:
			
				DAQmxErrChk (DAQmxCreateAOFuncGenChan(dev->AOTaskSet->taskHndl, (*chanSetPtr)->name, "", ,
				
				break;
			*/
			
			default:
				if (errorInfo)
				*errorInfo = FormatMsg(ConfigDAQmxAOTask_Err_ChannelNotImplemented, "ConfigDAQmxAOTask", "Channel type not implemented");
				return ConfigDAQmxAOTask_Err_ChannelNotImplemented;
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
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampClk_Rate, dev->AOTaskSet->timing->sampleRate));
	
	// set operation mode: finite, continuous
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampQuant_SampMode, dev->AOTaskSet->timing->measMode));
	
	// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
	// set number of samples per channel for finite generation
	if (dev->AOTaskSet->timing->measMode == MeasFinite)
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->AOTaskSet->timing->nSamples));
	
	// if a reference clock is given, use it to synchronize the internal clock
	if (dev->AOTaskSet->timing->refClkSource) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_RefClk_Src, dev->AOTaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_RefClk_Rate, dev->AOTaskSet->timing->refClkFreq));
	}
	
	// disable AO regeneration
	DAQmxSetWriteAttribute (dev->AOTaskSet->taskHndl, DAQmx_Write_RegenMode, DAQmx_Val_DoNotAllowRegen);
	
	
	switch (dev->AOTaskSet->timing->measMode) {
		case MeasFinite:
			// adjust output buffer size to match the number of samples to be generated   
		//	DAQmxErrChk ( DAQmxCfgOutputBuffer(dev->AOTaskSet->taskHndl, dev->AOTaskSet->timing->nSamples) );
		//	break;
			
		case MeasCont:
			// adjust output buffer size to be twice (even multiple) the blocksize
			DAQmxErrChk ( DAQmxCfgOutputBuffer(dev->AOTaskSet->taskHndl, 2 * dev->AOTaskSet->timing->blockSize));
			break;
	}
	
	
	//----------------------
	// Configure AO triggers
	//----------------------
	// configure start trigger
	if (dev->AOTaskSet->startTrig)
		switch (dev->AOTaskSet->startTrig->trigType) {
			case Trig_None:
				DAQmxErrChk (DAQmxDisableStartTrig(dev->AOTaskSet->taskHndl));   
				break;
			
			case Trig_AnalogEdge:
				if (dev->AOTaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->slope, dev->AOTaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AOTaskSet->startTrig->trigSource) 
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->wndTrigCond, 
							 dev->AOTaskSet->startTrig->wndTop, dev->AOTaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AOTaskSet->startTrig->trigSource) 
					DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	// configure reference trigger
	if (dev->AOTaskSet->referenceTrig)
		switch (dev->AOTaskSet->referenceTrig->trigType) {
				
			case Trig_None:
				DAQmxErrChk (DAQmxDisableRefTrig(dev->AOTaskSet->taskHndl));   
				break;
			
			case Trig_AnalogEdge:
				if (dev->AOTaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->slope, 
														 dev->AOTaskSet->referenceTrig->level, dev->AOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AOTaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->wndTrigCond, 
							 dev->AOTaskSet->referenceTrig->wndTop, dev->AOTaskSet->referenceTrig->wndBttm, dev->AOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AOTaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgDigEdgeRefTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->slope, dev->AOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	//----------------------
	// Add AO Task callbacks
	//----------------------
	// register AO data request callback if task is continuous
	//if (dev->AOTaskSet->timing->measMode == MeasCont)
		DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent(dev->AOTaskSet->taskHndl, DAQmx_Val_Transferred_From_Buffer, dev->AOTaskSet->timing->blockSize, 0, AODAQmxTaskDataRequest_CB, dev)); 
	// register AO task done event callback
	// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
	// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
	DAQmxErrChk (DAQmxRegisterDoneEvent(dev->AOTaskSet->taskHndl, 0, AODAQmxTaskDone_CB, dev));
	
	//----------------------  
	// Commit AO Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	nullChk( errMsg = malloc((buffsize+1)*sizeof(char)) );
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ConfigDAQmxAOTask", errMsg);
	OKfree(errMsg);
	
	return error;
}

static int ConfigDAQmxDITask (Dev_type* dev, char** errorInfo)
{
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtr;
	
	if (!dev->DITaskSet) return 0; 		// do nothing
	
	// clear DI task if any
	if (dev->DITaskSet->taskHndl) {
		DAQmxErrChk(DAQmxClearTask(dev->DITaskSet->taskHndl));
		dev->DITaskSet->taskHndl = NULL;
	}
	
	// check if there is at least one DI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nChans			= ListNumItems(dev->DITaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the DI task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create DAQmx DI task 
	char* taskName = GetTaskControlName(dev->taskController);
	AppendString(&taskName, " DI Task", -1);
	DAQmxErrChk (DAQmxCreateTask(taskName, &dev->DITaskSet->taskHndl));
	OKfree(taskName);
	
	// create DI channels
	nChans	= ListNumItems(dev->DITaskSet->chanSet); 
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		
		DAQmxErrChk (DAQmxCreateDIChan(dev->DITaskSet->taskHndl, (*chanSetPtr)->name, "", DAQmx_Val_ChanForAllLines)); 
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
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampClk_Rate, dev->DITaskSet->timing->sampleRate));
	
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
		int			quot;
		uInt32		defaultBuffSize; 
		
		case MeasFinite:
			
			// set number of samples per channel read within one call of the read function, i.e. blocksize
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->DITaskSet->timing->nSamples));
			quot = dev->DITaskSet->timing->nSamples  / dev->DITaskSet->timing->blockSize; 
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
	if (dev->DITaskSet->startTrig)
		switch (dev->DITaskSet->startTrig->trigType) {
				
			case Trig_None:
				DAQmxErrChk (DAQmxDisableStartTrig(dev->DITaskSet->taskHndl));
				break;
			
			case Trig_AnalogEdge:
				if (dev->DITaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->slope, dev->DITaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DITaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->wndTrigCond, 
						 		dev->DITaskSet->startTrig->wndTop, dev->DITaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DITaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
	}
	
	// configure reference trigger
	if (dev->DITaskSet->referenceTrig)
		switch (dev->DITaskSet->referenceTrig->trigType) {
				
			case Trig_None:
				DAQmxErrChk (DAQmxDisableRefTrig(dev->DITaskSet->taskHndl)); 
				break;
			
			case Trig_AnalogEdge:
				if (dev->DITaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->slope, 
														 dev->DITaskSet->referenceTrig->level, dev->DITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DITaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->wndTrigCond, 
								 dev->DITaskSet->referenceTrig->wndTop, dev->DITaskSet->referenceTrig->wndBttm, dev->DITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DITaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgDigEdgeRefTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->slope, dev->DITaskSet->referenceTrig->nPreTrigSamples));  
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
	
	
	//----------------------  
	// Commit DI Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->DITaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->DITaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->DITaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;

DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ConfigDAQmxDITask", errMsg);
	free(errMsg);
	return error;	
}

static int ConfigDAQmxDOTask (Dev_type* dev, char** errorInfo)
{
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtr;
	
	#define ConfigDAQmxDOTask_Err_OutOfMemory				-1 
	
	if (!dev->DOTaskSet) return 0; 		// do nothing
	
	// clear DO task if any
	if (dev->DOTaskSet->taskHndl) {
		DAQmxErrChk(DAQmxClearTask(dev->DOTaskSet->taskHndl));
		dev->DOTaskSet->taskHndl = NULL;
	}
	
	// clear and init writeDOData used for continuous streaming
	discard_WriteDOData_type(&dev->DOTaskSet->writeDOData);
	dev->DOTaskSet->writeDOData = init_WriteDOData_type(dev);
	if (!dev->DOTaskSet->writeDOData) goto MemError;
						 
	// check if there is at least one DO task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nChans			= ListNumItems(dev->DOTaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
		if (!(*chanSetPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	// continue setting up the DO task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create DAQmx DO task 
	char* taskName = GetTaskControlName(dev->taskController);
	AppendString(&taskName, " DO Task", -1);
	DAQmxErrChk (DAQmxCreateTask(taskName, &dev->DOTaskSet->taskHndl));
	OKfree(taskName);
	
	// create DO channels
	nChans	= ListNumItems(dev->DOTaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		
		DAQmxErrChk (DAQmxCreateDOChan(dev->DOTaskSet->taskHndl, (*chanSetPtr)->name, "", DAQmx_Val_ChanForAllLines)); 
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
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampClk_Rate, dev->DOTaskSet->timing->sampleRate));
	
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
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->DOTaskSet->timing->nSamples));
	
	// disable DO regeneration
	DAQmxSetWriteAttribute (dev->DOTaskSet->taskHndl, DAQmx_Write_RegenMode, DAQmx_Val_DoNotAllowRegen);
	
	// adjust output buffer size to be even multiple of blocksize
	DAQmxErrChk (DAQmxCfgOutputBuffer(dev->DOTaskSet->taskHndl, 2 * dev->DOTaskSet->timing->blockSize));
	
	//----------------------
	// Configure DO triggers
	//----------------------
	// configure start trigger
	if (dev->DOTaskSet->startTrig)
		switch (dev->DOTaskSet->startTrig->trigType) {
				
			case Trig_None:
				DAQmxErrChk (DAQmxDisableStartTrig(dev->DOTaskSet->taskHndl));  
				break;
			
			case Trig_AnalogEdge:
				if (dev->DOTaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->slope, dev->DOTaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DOTaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->wndTrigCond, 
						 		dev->DOTaskSet->startTrig->wndTop, dev->DOTaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DOTaskSet->startTrig->trigSource)
					DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->slope));  
				break;
			
			case Trig_DigitalPattern:
				break;
	}
	
	// configure reference trigger
	if (dev->DOTaskSet->referenceTrig)
		switch (dev->DOTaskSet->referenceTrig->trigType) {
				
			case Trig_None:
				DAQmxErrChk (DAQmxDisableRefTrig(dev->DOTaskSet->taskHndl));     
				break;
			
			case Trig_AnalogEdge:
				if (dev->DOTaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->slope, 
														 dev->DOTaskSet->referenceTrig->level, dev->DOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DOTaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->wndTrigCond, 
						 		dev->DOTaskSet->referenceTrig->wndTop, dev->DOTaskSet->referenceTrig->wndBttm, dev->DOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DOTaskSet->referenceTrig->trigSource)
					DAQmxErrChk (DAQmxCfgDigEdgeRefTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->slope, dev->DOTaskSet->referenceTrig->nPreTrigSamples));  
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
	
				 
	//----------------------  
	// Commit DO Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->DOTaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->DOTaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->DOTaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;
	
MemError:
	
	if (errorInfo)
		*errorInfo = FormatMsg (ConfigDAQmxDOTask_Err_OutOfMemory, "ConfigDAQmxAOTask", "Out of memory");
	return ConfigDAQmxDOTask_Err_OutOfMemory;

DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ConfigDAQmxDOTask", errMsg);
	free(errMsg);
	return error;
}

static int ConfigDAQmxCITask (Dev_type* dev, char** errorInfo)
{
#define ConfigDAQmxCITask_Err_ChanNotImplemented		-1 
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtr;
	size_t				nCIChan;
	char*				refclksource;
	char*				sampleclksource;
	
	if (!dev->CITaskSet) return 0;	// do nothing
	
	// get number of CI channels
	nCIChan = ListNumItems(dev->CITaskSet->chanTaskSet); 
	
	// clear CI tasks
	for (size_t i = 1; i <= nCIChan; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		if ((*chanSetPtr)->taskHndl) {
			DAQmxErrChk(DAQmxClearTask((*chanSetPtr)->taskHndl));
			(*chanSetPtr)->taskHndl = NULL;
		}
	}
	
	// check if there is at least one CI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	for (size_t i = 1; i <= nCIChan; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		if (!(*chanSetPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	
	// continue setting up the CI task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create CI channels
	for (size_t i = 1; i <= nCIChan; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
	
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		
		// create DAQmx CI task for each counter
		char* taskName = GetTaskControlName(dev->taskController);
		AppendString(&taskName, " CI Task on ", -1);
		AppendString(&taskName, strstr((*chanSetPtr)->name, "/")+1, -1);
		DAQmxErrChk (DAQmxCreateTask(taskName, &(*chanSetPtr)->taskHndl));
		OKfree(taskName);
				
		switch ((*chanSetPtr)->chanType) {
				
			case Chan_CI_Frequency:
				
				ChanSet_CI_Frequency_type*	chCIFreqPtr = *(ChanSet_CI_Frequency_type**)chanSetPtr;
				
				sampleclksource=chCIFreqPtr->baseClass.startTrig->trigSource;
				refclksource=chCIFreqPtr->baseClass.referenceTrig->trigSource;
				
				DAQmxErrChk (DAQmxCreateCIFreqChan((*chanSetPtr)->taskHndl, (*chanSetPtr)->name, "", chCIFreqPtr->freqMin, chCIFreqPtr->freqMax, DAQmx_Val_Hz, 
												   chCIFreqPtr->edgeType, chCIFreqPtr->measMethod, chCIFreqPtr->measTime, chCIFreqPtr->divisor, NULL));
				
				// use another terminal for the counter if other than default
				if (chCIFreqPtr->freqInputTerminal)
				DAQmxErrChk (DAQmxSetChanAttribute((*chanSetPtr)->taskHndl, (*chanSetPtr)->name, DAQmx_CI_Freq_Term, chCIFreqPtr->freqInputTerminal));
				
				// sample clock source
				// if no sample clock is given, then use OnboardClock by default
				if (!sampleclksource)  //chCIFreqPtr->taskTiming->sampClkSource
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_SampClk_Src, "OnboardClock"));
				else
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_SampClk_Src, sampleclksource));   //chCIFreqPtr->taskTiming->sampClkSource
	
				// sample clock edge
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_SampClk_ActiveEdge, chCIFreqPtr->baseClass.startTrig->slope));  //chCIFreqPtr->taskTiming->sampClkEdge
	
				// sampling rate
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_SampClk_Rate, *chCIFreqPtr->taskTiming->refSampleRate));
	
				// set operation mode: finite, continuous
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_SampQuant_SampMode, chCIFreqPtr->taskTiming->measMode));
	
				// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
				DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
				// if a reference clock is given, use it to synchronize the internal clock
				if (refclksource) {  //chCIFreqPtr->taskTiming->refClkSource
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Src,refclksource ));	  //chCIFreqPtr->taskTiming->refClkSource
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->taskTiming->refClkFreq));
				}
	
				// set number of samples per channel for finite acquisition
				if (chCIFreqPtr->taskTiming->measMode == MeasFinite)
					DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_SampQuant_SampPerChan, *chCIFreqPtr->taskTiming->refNSamples));
				
				/*
				DAQmxErrChk (DAQmxSetTrigAttribute(gTaskHandle,DAQmx_ArmStartTrig_Type,DAQmx_Val_DigEdge));
				DAQmxErrChk (DAQmxSetTrigAttribute(gTaskHandle,DAQmx_DigEdge_ArmStartTrig_Src,sampleClkSrc));
				DAQmxErrChk (DAQmxSetTrigAttribute(gTaskHandle,DAQmx_DigEdge_ArmStartTrig_Edge,DAQmx_Val_Rising));
				*/
				
				
				break;
				
			default:
				if (errorInfo)
					*errorInfo = FormatMsg(ConfigDAQmxCITask_Err_ChanNotImplemented, "ConfigDAQmxCITask", "Channel type not implemented");
				return ConfigDAQmxCITask_Err_ChanNotImplemented;
		}
		
		//----------------------  
		// Commit CI Task
		//----------------------  
	
		DAQmxErrChk( DAQmxTaskControl((*chanSetPtr)->taskHndl, DAQmx_Val_Task_Verify) );
		DAQmxErrChk( DAQmxTaskControl((*chanSetPtr)->taskHndl, DAQmx_Val_Task_Reserve) );
		DAQmxErrChk( DAQmxTaskControl((*chanSetPtr)->taskHndl, DAQmx_Val_Task_Commit) );
	}
				 
	
	return 0;

DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ConfigDAQmxCITask", errMsg);
	free(errMsg);
	return error;
}

static int ConfigDAQmxCOTask (Dev_type* dev, char** errorInfo)
{
#define ConfigDAQmxCOTask_Err_ChanNotImplemented		-1 
	
	int 				error 			= 0;
	ChanSet_type** 		chanSetPtr;
	ChanSet_CO_type*	chCOPtr;
	size_t				nCOChan;
	
	if (!dev->COTaskSet) return 0;	// do nothing
	
	// get number of CO channels
	nCOChan = ListNumItems(dev->COTaskSet->chanTaskSet); 
	
	// clear CO tasks
	for (size_t i = 1; i <= nCOChan; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		if ((*chanSetPtr)->taskHndl) {
			DAQmxErrChk(DAQmxClearTask((*chanSetPtr)->taskHndl));
			(*chanSetPtr)->taskHndl = NULL;
		}
	}
	
	// check if there is at least one CO task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	for (size_t i = 1; i <= nCOChan; i++) {	
		chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		if (!(*chanSetPtr)->onDemand) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	
	// continue setting up the CO task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create CO channels
	for (size_t i = 1; i <= nCOChan; i++) {	
		chanSetPtr 	= ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		chCOPtr 		= *(ChanSet_CO_type**)chanSetPtr;  
		// include in the task only channel for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		
		
		char* taskName = GetTaskControlName(dev->taskController);
		AppendString(&taskName, " CO Task on ", -1);
		AppendString(&taskName, strstr((*chanSetPtr)->name, "/")+1, -1);
		
		// create DAQmx CO task for each counter 
		DAQmxErrChk (DAQmxCreateTask(taskName, &(*chanSetPtr)->taskHndl));
		OKfree(taskName);
				
		switch ((*chanSetPtr)->chanType) {
				
			case Chan_CO_Pulse_Frequency:
				
				DAQmxErrChk ( DAQmxCreateCOPulseChanFreq((*chanSetPtr)->taskHndl, (*chanSetPtr)->name, "", DAQmx_Val_Hz, GetPulseTrainIdleState(chCOPtr->pulseTrain), 
														 GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)chCOPtr->pulseTrain), GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)chCOPtr->pulseTrain), 
														 (GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)chCOPtr->pulseTrain)/100)) );
	
				
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCOPtr->baseClass.referenceTrig->trigType==DAQmx_Val_DigEdge) {
					DAQmxErrChk ( DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Src, chCOPtr->baseClass.referenceTrig->trigSource) );
					//DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->timing->refClkFreq));
				}
					// if no reference clock is given, then use OnboardClock by default
			
	
			
				// trigger 	
				if (chCOPtr->baseClass.startTrig->trigType == DAQmx_Val_DigEdge){
					if (chCOPtr->baseClass.startTrig->trigSource)
						DAQmxErrChk ( DAQmxCfgDigEdgeStartTrig((*chanSetPtr)->taskHndl, chCOPtr->baseClass.startTrig->trigSource, chCOPtr->baseClass.startTrig->slope) );  
					else
						DAQmxErrChk ( DAQmxDisableStartTrig((*chanSetPtr)->taskHndl) );
				}
			//	else 
			//		DAQmxErrChk (DAQmxSetTrigAttribute((*chanSetPtr)->taskHndl,DAQmx_ArmStartTrig_Type,DAQmx_Val_None));  
			
				DAQmxErrChk ( DAQmxCfgImplicitTiming((*chanSetPtr)->taskHndl, GetPulseTrainMode(chCOPtr->pulseTrain), GetPulseTrainNPulses(chCOPtr->pulseTrain)) );
				DAQmxErrChk ( DAQmxSetTrigAttribute((*chanSetPtr)->taskHndl,DAQmx_StartTrig_Retriggerable, TRUE));    
				break;
				
			case Chan_CO_Pulse_Time:
				     
				DAQmxErrChk ( DAQmxCreateCOPulseChanTime((*chanSetPtr)->taskHndl, (*chanSetPtr)->name, "",DAQmx_Val_Seconds  , GetPulseTrainIdleState(chCOPtr->pulseTrain), 
													GetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)chCOPtr->pulseTrain), GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)chCOPtr->pulseTrain), 
													GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)chCOPtr->pulseTrain)) );
	
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCOPtr->baseClass.referenceTrig->trigType==DAQmx_Val_DigEdge) {
					DAQmxErrChk ( DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Src, chCOPtr->baseClass.referenceTrig->trigSource) );
			//		DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->timing->refClkFreq));
				}
					// if no reference clock is given, then use OnboardClock by default
			
	
				// trigger 	
				if (chCOPtr->baseClass.startTrig->trigType==DAQmx_Val_DigEdge){
					if (chCOPtr->baseClass.startTrig->trigSource)
						DAQmxErrChk ( DAQmxCfgDigEdgeStartTrig((*chanSetPtr)->taskHndl, chCOPtr->baseClass.startTrig->trigSource, chCOPtr->baseClass.startTrig->slope) );  
					else
						DAQmxErrChk ( DAQmxDisableStartTrig((*chanSetPtr)->taskHndl) );
				}
			
				DAQmxErrChk ( DAQmxCfgImplicitTiming((*chanSetPtr)->taskHndl, GetPulseTrainMode(chCOPtr->pulseTrain), GetPulseTrainNPulses(chCOPtr->pulseTrain)) ); 
				DAQmxErrChk ( DAQmxSetTrigAttribute((*chanSetPtr)->taskHndl,DAQmx_StartTrig_Retriggerable, TRUE) );    
				break;
				
			case Chan_CO_Pulse_Ticks:
				
				DAQmxErrChk ( DAQmxCreateCOPulseChanTicks((*chanSetPtr)->taskHndl, (*chanSetPtr)->name, "",chCOPtr->baseClass.referenceTrig->trigSource,
							 GetPulseTrainIdleState(chCOPtr->pulseTrain), GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)chCOPtr->pulseTrain), GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)chCOPtr->pulseTrain),
							 GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)chCOPtr->pulseTrain)) );
	
				
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCOPtr->baseClass.referenceTrig->trigType==DAQmx_Val_DigEdge) {
					DAQmxErrChk ( DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Src, chCOPtr->baseClass.referenceTrig->trigSource) );
			//		DAQmxErrChk (DAQmxSetTimingAttribute((*chanSetPtr)->taskHndl, DAQmx_RefClk_Rate, chCIFreqPtr->timing->refClkFreq));
				}
					// if no reference clock is given, then use OnboardClock by default
			
	
			
				// trigger 	
				if (chCOPtr->baseClass.startTrig->trigType==DAQmx_Val_DigEdge){
					if (chCOPtr->baseClass.startTrig->trigSource)
						DAQmxErrChk ( DAQmxCfgDigEdgeStartTrig((*chanSetPtr)->taskHndl, chCOPtr->baseClass.startTrig->trigSource, chCOPtr->baseClass.startTrig->slope) );  
					else
						DAQmxErrChk ( DAQmxDisableStartTrig((*chanSetPtr)->taskHndl) ); 	
				}
			
				DAQmxErrChk ( DAQmxCfgImplicitTiming((*chanSetPtr)->taskHndl, GetPulseTrainMode(chCOPtr->pulseTrain), GetPulseTrainNPulses(chCOPtr->pulseTrain)) );    
				DAQmxErrChk ( DAQmxSetTrigAttribute((*chanSetPtr)->taskHndl,DAQmx_StartTrig_Retriggerable, TRUE) );      
				
			break;
				
			default:
				if (errorInfo)
					*errorInfo = FormatMsg(ConfigDAQmxCOTask_Err_ChanNotImplemented, "ConfigDAQmxCOTask", "Channel type not implemented");
				return ConfigDAQmxCOTask_Err_ChanNotImplemented;
		}
		
		// register CO task done event callback
		// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
		// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
		DAQmxErrChk (DAQmxRegisterDoneEvent((*chanSetPtr)->taskHndl, 0, CODAQmxTaskDone_CB, dev));
		
		//----------------------  
		// Commit CO Task
		//----------------------  
	
		DAQmxErrChk( DAQmxTaskControl((*chanSetPtr)->taskHndl, DAQmx_Val_Task_Verify) );
		DAQmxErrChk( DAQmxTaskControl((*chanSetPtr)->taskHndl, DAQmx_Val_Task_Reserve) );
		DAQmxErrChk( DAQmxTaskControl((*chanSetPtr)->taskHndl, DAQmx_Val_Task_Commit) );
	}
	
	
	return 0;

DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "ConfigDAQmxCOTask", errMsg);
	free(errMsg);
	return error;
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
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems(dev->CITaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if ((*chanSetPtr)->taskHndl) {
				DAQmxClearTask((*chanSetPtr)->taskHndl);
				(*chanSetPtr)->taskHndl = NULL;
			}
		}
	}
	
	// CO task
	if (dev->COTaskSet) {
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems(dev->COTaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);	
			if ((*chanSetPtr)->taskHndl) {
				DAQmxClearTask((*chanSetPtr)->taskHndl);
				(*chanSetPtr)->taskHndl = NULL;
			}
		}
	}
	
	// signal that device is not configured
	TaskControlEvent(dev->taskController, TASK_EVENT_UNCONFIGURE, NULL, NULL);
	
	return 0;
	
Error:
	DisplayLastDAQmxLibraryError();
	return error;
}

static int StopDAQmxTasks (Dev_type* dev, char** errorInfo)
{
#define StopDAQmxTasks_Err_StoppingTasks	-1
	
	int 				error				= 0;  
	int*				nActiveTasksPtr		= NULL;
	bool32				taskDoneFlag		= FALSE;
	DataPacket_type*	nullPacket			= NULL;
	char*				errMsg				= NULL;
	
	CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
	// AI task
	if (dev->AITaskSet && dev->AITaskSet->taskHndl) {
		DAQmxIsTaskDone(dev->AITaskSet->taskHndl, &taskDoneFlag); 
		if (!taskDoneFlag ) {
			errChk(DAQmxStopTask(dev->AITaskSet->taskHndl));
			(*nActiveTasksPtr)--;
			
			// send NULL data packets to AI channels used in the DAQmx task
			size_t				nChans			= ListNumItems(dev->AITaskSet->chanSet); 
			ChanSet_type**		chanSetPtr; 
			for (size_t i = 1; i <= nChans; i++) { 
				chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
				// include only channels for which HW-timing is required
				if ((*chanSetPtr)->onDemand) continue;
				// send NULL packet to signal end of data transmission
				errChk( SendDataPacket((*chanSetPtr)->srcVChan, &nullPacket, 0, &errMsg) );
			}
			
		}
	}
	
	// AO task (stop if finite; continuous task stopping happens on receiving a NULL packet and is handled by WriteAODAQmx)
	if (dev->AOTaskSet && dev->AOTaskSet->taskHndl && dev->AOTaskSet->timing->measMode == MeasFinite) {
		DAQmxIsTaskDone(dev->AOTaskSet->taskHndl, &taskDoneFlag); 
		if (!taskDoneFlag ) {
			errChk(DAQmxStopTask(dev->AOTaskSet->taskHndl));
			(*nActiveTasksPtr)--; 
		}
	}
	
	// DI task
	if (dev->DITaskSet && dev->DITaskSet->taskHndl) {
		DAQmxIsTaskDone(dev->DITaskSet->taskHndl, &taskDoneFlag); 
		if (!taskDoneFlag ) {
			errChk(DAQmxStopTask(dev->DITaskSet->taskHndl));
			(*nActiveTasksPtr)--;
		}
	}
	
	// DO task
	if (dev->DOTaskSet && dev->DOTaskSet->taskHndl) {
		DAQmxIsTaskDone(dev->DOTaskSet->taskHndl, &taskDoneFlag); 
		if (!taskDoneFlag ) {
			errChk(DAQmxStopTask(dev->DOTaskSet->taskHndl));
			(*nActiveTasksPtr)--;
		}
	}
	
	// CI task
	if (dev->CITaskSet) {
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems(dev->CITaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if ((*chanSetPtr)->taskHndl) {
				DAQmxIsTaskDone((*chanSetPtr)->taskHndl, &taskDoneFlag); 
				if (!taskDoneFlag ) {
					errChk( DAQmxStopTask((*chanSetPtr)->taskHndl) );
					(*nActiveTasksPtr)--;
				}
			}
		}
	}
	
	// CO task
	if (dev->COTaskSet) {
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems(dev->COTaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);	
			if ((*chanSetPtr)->taskHndl) {
				DAQmxIsTaskDone((*chanSetPtr)->taskHndl, &taskDoneFlag); 
				if (!taskDoneFlag ) {
					errChk( DAQmxStopTask((*chanSetPtr)->taskHndl) );
					(*nActiveTasksPtr)--;
				}
			}
		}
	}
	
	if (!*nActiveTasksPtr)
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
	CmtReleaseTSVPtr(dev->nActiveTasks);
	
	return 0;
	
Error:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	if (errorInfo)
		*errorInfo = FormatMsg(StopDAQmxTasks_Err_StoppingTasks, "StopDAQmxTasks", errMsg);
	OKfree(errMsg);
	return StopDAQmxTasks_Err_StoppingTasks;
}

static int StartAIDAQmxTask (Dev_type* dev, char** errorInfo)
{
	int 	error 										= 0;
	char	CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE];
	
	// launch AI task in a new thread if it exists
	if (!dev->AITaskSet) return 0;
	if (!dev->AITaskSet->taskHndl) return 0; 
	
	if ((error = CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartAIDAQmxTask_CB, dev, NULL)) < 0) goto CmtError;
	
	return 0;
	
CmtError:
	
	CmtGetErrorMessage (error, CmtErrMsgBuffer);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "StartAIDAQmxTask", CmtErrMsgBuffer);   
	return error;
}
							  
int CVICALLBACK StartAIDAQmxTask_CB (void *functionData)
{
	Dev_type*			dev				= functionData;
	char*				errMsg			= NULL;
	int					error			= 0;
	DataPacket_type*	dataPacket		= NULL;
	void*				dataPacketData	= NULL;
	DLDataTypes			dataPacketType;
	uInt64*				nSamplesPtr		= NULL;
	double*				samplingRatePtr	= NULL;
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	//----------
	// N samples
	//----------
	if (dev->AITaskSet->timing->measMode == MeasFinite) {
		if (IsVChanConnected((VChan_type*)dev->AITaskSet->timing->nSamplesSinkVChan)) {
			errChk( GetDataPacket(dev->AITaskSet->timing->nSamplesSinkVChan, &dataPacket, &errMsg) );
			dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType );
			switch (dataPacketType) {
				case DL_UChar:
					dev->AITaskSet->timing->nSamples = (uInt64)**(unsigned char**)dataPacketData;
					break;
				case DL_UShort:
					dev->AITaskSet->timing->nSamples = (uInt64)**(unsigned short**)dataPacketData;
					break;
				case DL_UInt:
					dev->AITaskSet->timing->nSamples = (uInt64)**(unsigned int**)dataPacketData;
					break;
				case DL_ULongLong:
					dev->AITaskSet->timing->nSamples = (uInt64)**(unsigned long long**)dataPacketData;
					break;
			}
			
			// update number of samples in dev structure
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->AITaskSet->timing->nSamples * dev->AITaskSet->timing->oversampling));
			// update number of samples in UI
			SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_NSamples, dev->AITaskSet->timing->nSamples);
			// update duration in UI
			SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_Duration, dev->AITaskSet->timing->nSamples / dev->AITaskSet->timing->sampleRate);
	
			// cleanup
			ReleaseDataPacket(&dataPacket);
		}
	} else
		// n samples cannot be used for continuous tasks, empty Sink VChan if there are any elements
		ReleaseAllDataPackets(dev->AITaskSet->timing->nSamplesSinkVChan, NULL);
	
	//--------------
	// Sampling rate
	//--------------
	if (IsVChanConnected((VChan_type*)dev->AITaskSet->timing->samplingRateSinkVChan)) { 
		errChk( GetDataPacket(dev->AITaskSet->timing->samplingRateSinkVChan, &dataPacket, &errMsg) );
		dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType);
		
		switch (dataPacketType) {
			case DL_Float:
				dev->AITaskSet->timing->sampleRate = (double)**(float**)dataPacketData;
				break;
				
			case DL_Double:
				dev->AITaskSet->timing->sampleRate = **(double**)dataPacketData;
				break;
		}
		
		// update sampling rate in dev structure
		DAQmxErrChk(DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_Rate, dev->AITaskSet->timing->sampleRate * dev->AITaskSet->timing->oversampling));
		// update sampling rate in UI
		SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_SamplingRate, dev->AITaskSet->timing->sampleRate);
		// update duration in UI
		SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_Duration, dev->AITaskSet->timing->nSamples / dev->AITaskSet->timing->sampleRate);
		// cleanup
		ReleaseDataPacket(&dataPacket);
	}
	
											
	//-------------------------------------------------------------------------------------------------------------------------------
	// Send task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	//----------
	// N samples
	//----------
	nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
	*nSamplesPtr = dev->AITaskSet->timing->nSamples;
	dataPacket = init_DataPacket_type(DL_ULongLong, &nSamplesPtr, NULL, NULL);
	errChk(SendDataPacket(dev->AITaskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errMsg));
	
	//--------------
	// Sampling rate
	//--------------
	nullChk( samplingRatePtr = malloc(sizeof(double)) );
	*samplingRatePtr = dev->AITaskSet->timing->sampleRate;
	dataPacket = init_DataPacket_type(DL_Double, &samplingRatePtr, NULL, NULL);
	errChk(SendDataPacket(dev->AITaskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errMsg));
	
	
	// reset AI data processing structure
	discard_ReadAIData_type(&dev->AITaskSet->readAIData);
	nullChk( dev->AITaskSet->readAIData = init_ReadAIData_type(dev) );
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Start task as a function of HW trigger dependencies
	//-------------------------------------------------------------------------------------------------------------------------------
	
	errChk(WaitForHWTrigArmedSlaves(dev->AITaskSet->HWTrigMaster, &errMsg));
	
	DAQmxErrChk(DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Start));
	
	errChk(SetHWTrigSlaveArmedStatus(dev->AITaskSet->HWTrigSlave, &errMsg));
	
	
	return 0;
	
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	// fall through
	
Error:
	
	if (!errMsg)
		errMsg = FormatMsg(error, "StartAIDAQmxTask_CB", "Out of memory");
	
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	OKfree(errMsg);
	return 0;

}

static int StartAODAQmxTask (Dev_type* dev, char** errorInfo)
{
	int 	error 										= 0;
	char	CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE];
	
	// launch AO task in a new thread if it exists
	if (!dev->AOTaskSet) return 0;
	if (!dev->AOTaskSet->taskHndl) return 0; 
	
	if ((error = CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartAODAQmxTask_CB, dev, NULL)) < 0) goto CmtError;
	
	return 0;
	
CmtError:
	
	CmtGetErrorMessage (error, CmtErrMsgBuffer);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "StartAODAQmxTask", CmtErrMsgBuffer);   
	return error;
	
}

int CVICALLBACK StartAODAQmxTask_CB (void *functionData)
{
#define StartAODAQmxTask_CB_Err_NumberOfReceivedSamplesNotTheSameAsTask		-1
	Dev_type*			dev						= functionData;
	char*				errMsg					= NULL;
	int					error					= 0;
	DataPacket_type*	dataPacket				= NULL;
	void*				dataPacketData			= NULL;
	DLDataTypes			dataPacketType;
	uInt64*				nSamplesPtr				= NULL;
	double*				samplingRatePtr			= NULL;
	Waveform_type*		AOWaveform				= NULL;
	float64*			AOData 					= NULL;
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	//----------
	// N samples
	//----------
	if (dev->AOTaskSet->timing->measMode == MeasFinite) {
		if (IsVChanConnected((VChan_type*)dev->AOTaskSet->timing->nSamplesSinkVChan)) {
			errChk( GetDataPacket(dev->AOTaskSet->timing->nSamplesSinkVChan, &dataPacket, &errMsg) );
			dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType );
			switch (dataPacketType) {
				case DL_UChar:
					dev->AOTaskSet->timing->nSamples = (uInt64)**(unsigned char**)dataPacketData;
					break;
				case DL_UShort:
					dev->AOTaskSet->timing->nSamples = (uInt64)**(unsigned short**)dataPacketData;
					break;
				case DL_UInt:
					dev->AOTaskSet->timing->nSamples = (uInt64)**(unsigned int**)dataPacketData;
					break;
				case DL_ULongLong:
					dev->AOTaskSet->timing->nSamples = (uInt64)**(unsigned long long**)dataPacketData;
					break;
			}
			
			// update number of samples in dev structure
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->AOTaskSet->timing->nSamples));
			// update number of samples in UI
			SetCtrlVal(dev->AOTaskSet->timing->settingsPanHndl, Set_NSamples, dev->AOTaskSet->timing->nSamples);
			// update duration in UI
			SetCtrlVal(dev->AOTaskSet->timing->settingsPanHndl, Set_Duration, dev->AOTaskSet->timing->nSamples / dev->AOTaskSet->timing->sampleRate);
	
			// cleanup
			ReleaseDataPacket(&dataPacket);
		}
	} else
		// n samples cannot be used for continuous tasks, empty Sink VChan if there are any elements
		ReleaseAllDataPackets(dev->AOTaskSet->timing->nSamplesSinkVChan, NULL);
	
	//--------------
	// Sampling rate
	//--------------
	if (IsVChanConnected((VChan_type*)dev->AOTaskSet->timing->samplingRateSinkVChan)) { 
		errChk( GetDataPacket(dev->AOTaskSet->timing->samplingRateSinkVChan, &dataPacket, &errMsg) );
		dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType);
		
		switch (dataPacketType) {
			case DL_Float:
				dev->AOTaskSet->timing->sampleRate = (double)**(float**)dataPacketData;
				break;
				
			case DL_Double:
				dev->AOTaskSet->timing->sampleRate = **(double**)dataPacketData;
				break;
		}
		
		// update sampling rate in dev structure
		DAQmxErrChk(DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampClk_Rate, dev->AOTaskSet->timing->sampleRate));
		// update sampling rate in UI
		SetCtrlVal(dev->AOTaskSet->timing->settingsPanHndl, Set_SamplingRate, dev->AOTaskSet->timing->sampleRate);
		// update duration in UI
		SetCtrlVal(dev->AOTaskSet->timing->settingsPanHndl, Set_Duration, dev->AOTaskSet->timing->nSamples / dev->AOTaskSet->timing->sampleRate);
		// cleanup
		ReleaseDataPacket(&dataPacket);
	}
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Send task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	//----------
	// N samples
	//----------
	nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
	*nSamplesPtr = dev->AOTaskSet->timing->nSamples;
	dataPacket = init_DataPacket_type(DL_ULongLong, &nSamplesPtr, NULL, NULL);
	errChk(SendDataPacket(dev->AOTaskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errMsg));
	
	//--------------
	// Sampling rate
	//--------------
	nullChk( samplingRatePtr = malloc(sizeof(double)) );
	*samplingRatePtr = dev->AOTaskSet->timing->sampleRate;
	dataPacket = init_DataPacket_type(DL_Double, &samplingRatePtr, NULL, NULL);
	errChk(SendDataPacket(dev->AOTaskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errMsg));
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Fill output buffer
	//-------------------------------------------------------------------------------------------------------------------------------
	
	switch (dev->AOTaskSet->timing->measMode) {
			
		case MeasFinite:
			/*
			// adjust output buffer size to match the number of samples to be generated   
			DAQmxErrChk( DAQmxCfgOutputBuffer(dev->AOTaskSet->taskHndl, dev->AOTaskSet->timing->nSamples) );
			
			// fill buffer with waveform data
			// note: only waveform are allowed in this mode
			size_t				nChannels 		= ListNumItems(dev->AOTaskSet->chanSet);
			ChanSet_type** 		chanSetPtr;
			size_t				nUsedAOChannels	= 0;
			void**				waveformDataPtr;
			size_t				nWaveformSamples;
			WaveformTypes		waveformType;
			
			for (size_t i = 1; i <= nChannels; i++) {
				chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
				if ((*chanSetPtr)->onDemand) continue;
				nUsedAOChannels++;
				
				// allocate memory to generate samples
				nullChk( AOData = realloc(AOData, nUsedAOChannels * dev->AOTaskSet->timing->nSamples * sizeof(float64)) ); 
			
				// receive waform data, if data is not waveform, error occurs
				errChk( ReceiveWaveform((*chanSetPtr)->sinkVChan, &AOWaveform, &waveformType, &errMsg) );
				waveformDataPtr = GetWaveformPtrToData(AOWaveform, &nWaveformSamples); 
				
				// check if number of samples in the waveform is equal to the number of samples set for the task
				if (nWaveformSamples != dev->AOTaskSet->timing->nSamples) {
					error = StartAODAQmxTask_CB_Err_NumberOfReceivedSamplesNotTheSameAsTask;
					errMsg = FormatMsg(error, "", "Number of received samples for finite AO is not the same as the number of samples set for the AO task");
					goto Error;
				}
				
				// copy data (only these two data types are used)
				switch (waveformType) {
					case Waveform_Double:
						memcpy(AOData + ((nUsedAOChannels-1) * dev->AOTaskSet->timing->nSamples), *waveformDataPtr, dev->AOTaskSet->timing->nSamples * sizeof(float64));
						break;
						
					case Waveform_Float:
						float* floatWaveformData = (float*)*waveformDataPtr;
						for (size_t j = 0; j < dev->AOTaskSet->timing->nSamples; j++)
							*(AOData + ((nUsedAOChannels-1)*dev->AOTaskSet->timing->nSamples) + j) = (float64) floatWaveformData[j];
						break;
						
				}
			}
			// write samples to AO buffer
			int	nSamplesWritten;
			DAQmxErrChk( DAQmxWriteAnalogF64(dev->AOTaskSet->taskHndl, dev->AOTaskSet->timing->nSamples, 0, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, AOData, &nSamplesWritten, NULL) );
			// cleanup
			OKfree(AOData);
			discard_Waveform_type(&AOWaveform); 
			break;
			*/
			
		case MeasCont:
			
			// clear streaming data structure
			discard_WriteAOData_type(&dev->AOTaskSet->writeAOData);
			nullChk( dev->AOTaskSet->writeAOData = init_WriteAOData_type(dev) );
			// output buffer is twice the writeblock, thus write two writeblocks
			errChk( WriteAODAQmx(dev, &errMsg) );
			errChk( WriteAODAQmx(dev, &errMsg) );
			
			break;
	}
									
	//-------------------------------------------------------------------------------------------------------------------------------
	// Start task as a function of HW trigger dependencies
	//-------------------------------------------------------------------------------------------------------------------------------
	
	errChk(WaitForHWTrigArmedSlaves(dev->AOTaskSet->HWTrigMaster, &errMsg));
	
	DAQmxErrChk(DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Start));
	
	errChk(SetHWTrigSlaveArmedStatus(dev->AOTaskSet->HWTrigSlave, &errMsg));
	
	
	return 0;
	
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	// fall through
	
Error:
	
	// cleanup
	OKfree(AOData);
	discard_Waveform_type(&AOWaveform);
	
	if (!errMsg)
		errMsg = FormatMsg(error, "StartAODAQmxTask_CB", "Out of memory");
	
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	OKfree(errMsg);
	return 0;
	
}

static int StartDIDAQmxTask (Dev_type* dev, char** errorInfo)
{
	return 0;	
}

int CVICALLBACK StartDIDAQmxTask_CB (void *functionData)
{
	return 0;	
}

static int StartDODAQmxTask (Dev_type* dev, char** errorInfo)
{
	return 0;	
}

int CVICALLBACK StartDODAQmxTask_CB (void *functionData)
{
	return 0;	
}

static int StartCIDAQmxTasks (Dev_type* dev, char** errorInfo)
{
	int 	error 										= 0;
	char	CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE];
	
	// launch CI tasks in a new thread if there are any
	if (!dev->CITaskSet) return 0;
	
	size_t				nCI 		= ListNumItems(dev->CITaskSet->chanTaskSet);
	ChanSet_type** 		chanSetPtr;
	for (size_t i = 1; i <= nCI; i++) {
		chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		if ((*chanSetPtr)->taskHndl)
			if ((error = CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartCIDAQmxTasks_CB, *chanSetPtr, NULL)) < 0) goto CmtError; 
	
	}
	
	return 0;
	
CmtError:
	
	CmtGetErrorMessage (error, CmtErrMsgBuffer);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "StartCIDAQmxTask", CmtErrMsgBuffer);   
	return error;		
}

int CVICALLBACK StartCIDAQmxTasks_CB (void *functionData)
{
	return 0;	
}

static int StartCODAQmxTasks (Dev_type* dev, char** errorInfo)
{
	int 	error 										= 0;
	char	CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE];
	
	// launch CO tasks in a new thread if there are any
	if (!dev->COTaskSet) return 0;
	
	size_t				nCO 		= ListNumItems(dev->COTaskSet->chanTaskSet);
	ChanSet_CO_type** 	chanSetCOPtr;
	for (size_t i = 1; i <= nCO; i++) {
		chanSetCOPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		if ((*chanSetCOPtr)->baseClass.taskHndl)
			if ((error = CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartCODAQmxTasks_CB, *chanSetCOPtr, NULL)) < 0) goto CmtError; 
	
	}
	
	return 0;
	
CmtError:
	
	CmtGetErrorMessage (error, CmtErrMsgBuffer);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "StartCODAQmxTask", CmtErrMsgBuffer);   
	return error;	
}

int CVICALLBACK StartCODAQmxTasks_CB (void *functionData)
{
	ChanSet_CO_type*	chanSetCO			= functionData;
	Dev_type*			dev 				= chanSetCO->baseClass.device;
	DataPacket_type*	dataPacket			= NULL;
	char*				errMsg				= NULL;
	int					error				= 0;
	void*				dataPacketData;
	DLDataTypes			dataPacketDataType;
	PulseTrain_type*	pulseTrain			= NULL;
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive task settings data and update UI settings
	//-------------------------------------------------------------------------------------------------------------------------------
	if (IsVChanConnected((VChan_type*)chanSetCO->baseClass.sinkVChan)) {
		errChk( GetDataPacket(chanSetCO->baseClass.sinkVChan, &dataPacket, &errMsg) );
		dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketDataType);
		discard_PulseTrain_type(&chanSetCO->pulseTrain);
		nullChk( chanSetCO->pulseTrain = CopyPulseTrain(*(PulseTrain_type**)dataPacketData) ); 
	
		// get channel panel handle to adjust UI settings
		int						timingPanHndl;
		
		GetPanelHandleFromTabPage(chanSetCO->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl); 
		
		double 					initialDelay;
		double					frequency;
		double					dutyCycle;
		double					lowTime;
		double					highTime;
		uInt32					initialDelayTicks;
		uInt32					lowTicks;
		uInt32					highTicks;
		PulseTrainIdleStates	idleState;
		PulseTrainModes			pulseMode;
		uInt64					nPulses;
		int						ctrlIdx;
		
		// set idle state
		idleState = GetPulseTrainIdleState(chanSetCO->pulseTrain);
		DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_IdleState, idleState) );
		// generation mode
		pulseMode = GetPulseTrainMode(chanSetCO->pulseTrain);
		// timing
		nPulses = GetPulseTrainNPulses(chanSetCO->pulseTrain);
		DAQmxErrChk ( DAQmxCfgImplicitTiming(chanSetCO->baseClass.taskHndl, pulseMode, nPulses) );
		
		switch (dataPacketDataType) {
			case DL_PulseTrain_Freq:
				// set initial delay
				initialDelay = GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq_InitialDelay, initialDelay) );
				SetCtrlVal(timingPanHndl, COFreqPan_InitialDelay, initialDelay); 
				// frequency
				frequency = GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq, frequency) );
				SetCtrlVal(timingPanHndl, COFreqPan_Frequency, frequency); 
				// duty cycle
				dutyCycle = GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_DutyCyc, dutyCycle/100) ); // normalize from [%] to 0-1 range
				SetCtrlVal(timingPanHndl, COFreqPan_DutyCycle, dutyCycle); // display in [%] 
				// set UI idle state
				switch (idleState) {
					case PulseTrainIdle_Low:
						SetCtrlIndex(timingPanHndl, COFreqPan_IdleState, 0);
						break;
						
					case PulseTrainIdle_High:
						SetCtrlIndex(timingPanHndl, COFreqPan_IdleState, 1);
						break;
				}
				// n pulses
				SetCtrlVal(timingPanHndl, COFreqPan_NPulses, nPulses);
				// pulse mode (daqmx settings set before)
				GetIndexFromValue(timingPanHndl, COFreqPan_Mode, &ctrlIdx, pulseMode); 
				SetCtrlIndex(timingPanHndl, COFreqPan_Mode, ctrlIdx);
				break;
				
			case DL_PulseTrain_Time:
				// set initial delay
				initialDelay = GetPulseTrainTimeTimingInitialDelay((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Time_InitialDelay, initialDelay) );
				SetCtrlVal(timingPanHndl, COTimePan_InitialDelay, initialDelay); 
				// low time
				lowTime = GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTime, lowTime) );
				SetCtrlVal(timingPanHndl, COTimePan_LowTime, lowTime); 
				// high time
				highTime = GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTime, highTime) );
				SetCtrlVal(timingPanHndl, COTimePan_HighTime, highTime);
				// set UI idle state
				switch (idleState) {
					case PulseTrainIdle_Low:
						SetCtrlIndex(timingPanHndl, COTimePan_IdleState, 0);
						break;
						
					case PulseTrainIdle_High:
						SetCtrlIndex(timingPanHndl, COTimePan_IdleState, 1);
						break;
				} 
				// n pulses
				SetCtrlVal(timingPanHndl, COTimePan_NPulses, nPulses);
				// pulse mode (daqmx settings set before)
				GetIndexFromValue(timingPanHndl, COTimePan_Mode, &ctrlIdx, pulseMode); 
				SetCtrlIndex(timingPanHndl, COTimePan_Mode, ctrlIdx);
				break;
			
			case DL_PulseTrain_Ticks:
				// tick delay
				initialDelayTicks = GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Ticks_InitialDelay, initialDelayTicks) );
				SetCtrlVal(timingPanHndl, COTicksPan_InitialDelay, initialDelayTicks); 
				// low ticks
				lowTicks = GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTicks, lowTicks) );
				SetCtrlVal(timingPanHndl, COTicksPan_LowTicks, lowTicks); 
				// high ticks
				highTicks = GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTicks, highTicks) );
				SetCtrlVal(timingPanHndl, COTicksPan_HighTicks, highTicks);
				// set UI idle state
				switch (idleState) {
					case PulseTrainIdle_Low:
						SetCtrlIndex(timingPanHndl, COTicksPan_IdleState, 0);
						break;
						
					case PulseTrainIdle_High:
						SetCtrlIndex(timingPanHndl, COTicksPan_IdleState, 1);
						break;
				}
				// n pulses
				SetCtrlVal(timingPanHndl, COTicksPan_NPulses, nPulses);
				// pulse mode (daqmx settings set before)
				GetIndexFromValue(timingPanHndl, COTicksPan_Mode, &ctrlIdx, pulseMode); 
				SetCtrlIndex(timingPanHndl, COTicksPan_Mode, ctrlIdx);
				break;
		}
	
		ReleaseDataPacket(&dataPacket);
		dataPacket = NULL;
	}
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Send task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	// copy pulse train info
	nullChk( pulseTrain = CopyPulseTrain(chanSetCO->pulseTrain) );
	
	// generate data packet
	switch (GetPulseTrainType(chanSetCO->pulseTrain)) {
		
		case PulseTrain_Freq:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, &pulseTrain, NULL,discard_PulseTrain_type) );  
			break;
			
		case PulseTrain_Time:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, &pulseTrain, NULL,discard_PulseTrain_type) ); 
			break;
			
		case PulseTrain_Ticks:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, &pulseTrain, NULL,discard_PulseTrain_type) ); 
			break;
		
	}
	
	errChk( SendDataPacket(chanSetCO->baseClass.srcVChan, &dataPacket, FALSE, &errMsg) );
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Start task as a function of HW trigger dependencies
	//-------------------------------------------------------------------------------------------------------------------------------
	
	errChk(WaitForHWTrigArmedSlaves(chanSetCO->baseClass.HWTrigMaster, &errMsg));
	
	DAQmxErrChk(DAQmxTaskControl(chanSetCO->baseClass.taskHndl, DAQmx_Val_Task_Start));
	
	errChk(SetHWTrigSlaveArmedStatus(chanSetCO->baseClass.HWTrigSlave, &errMsg));
	
	
	return 0;
	
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	// fall through
	
Error:
	
	// cleanup
	discard_PulseTrain_type(&pulseTrain);
	
	if (!errMsg)
		errMsg = FormatMsg(error, "StartCODAQmxTask_CB", "Out of memory");
	
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	OKfree(errMsg);
	return 0;
	
}

//---------------------------------------------------------------------------------------------------------------------
// DAQmx task callbacks
//---------------------------------------------------------------------------------------------------------------------

static int SendAIBufferData (Dev_type* dev, ChanSet_type* AIChSet, size_t chIdx, int nRead, float64* AIReadBuffer, char** errorInfo) 
{
	int					error					= 0;
	char*				errMsg					= NULL;
	double*				waveformData_double		= NULL;
	float*				waveformData_float		= NULL;
	uInt8*				waveformData_uInt8		= NULL;
	uInt16*				waveformData_uInt16		= NULL;
	uInt32*				waveformData_uInt32		= NULL;
	Waveform_type*		waveform				= NULL; 
	DataPacket_type*	dataPacket				= NULL;
	double				scalingFactor;
	double				scaledSignal;
	double				maxSignal;
	float64*			integrationBuffer		= dev->AITaskSet->readAIData->intBuffers[chIdx];
	uInt32				integration				= dev->AITaskSet->timing->oversampling;
	uInt32				nItegratedSamples		= (nRead + dev->AITaskSet->readAIData->nIntBuffElem[chIdx]) / integration;  // number of samples integrated per channel
	uInt32				nRemainingSamples		= (nRead + dev->AITaskSet->readAIData->nIntBuffElem[chIdx]) % integration;	// number of samples remaining to be integrated per channel
	uInt32				nBufferSamples;
	float64*			AIOffsetReadBuffer		= AIReadBuffer + chIdx * nRead;
	uInt32				i, j, k;
	
	switch(GetSourceVChanDataType(AIChSet->srcVChan)) {
				
		case DL_Waveform_Double:
			
			//----------------------
			// process incoming data
			//----------------------
			
			nullChk( waveformData_double = calloc(nItegratedSamples, sizeof(double)) );
			
			// add data to the integration buffer
			memcpy(integrationBuffer + dev->AITaskSet->readAIData->nIntBuffElem[chIdx], AIReadBuffer + chIdx * nRead, nRead * sizeof(float64));
			dev->AITaskSet->readAIData->nIntBuffElem[chIdx] += nRead;
			nBufferSamples = dev->AITaskSet->readAIData->nIntBuffElem[chIdx];
			
			if (AIChSet->integrateFlag)
				// integrate
				for (i = 0; i < integration; i++) {
					k = i;
					for (j = 0; j < nItegratedSamples; j++) {
						waveformData_double[j] += integrationBuffer[k] * AIChSet->gain + AIChSet->offset;
						k += integration;
					}							   
				}
			 else 
				// jump over oversampled samples
				for (i = 0; i < nItegratedSamples; i++)
					waveformData_double[i] = integrationBuffer[i*integration] * AIChSet->gain + AIChSet->offset;
			
			
			// shift unprocessed samples from the end of the buffer to its beginning
			if (nRemainingSamples)
				memmove(integrationBuffer, integrationBuffer + (nBufferSamples - nRemainingSamples), nRemainingSamples * sizeof(float64));
			
			dev->AITaskSet->readAIData->nIntBuffElem[chIdx] = nRemainingSamples;
			
			//-------------------- 
			// prepare data packet
			//--------------------
			nullChk( waveform 	= init_Waveform_type(Waveform_Double, dev->AITaskSet->timing->sampleRate, nItegratedSamples, (void**)&waveformData_double) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_Double, &waveform,  NULL,(DiscardPacketDataFptr_type) discard_Waveform_type) ); 
				
			break;
				
		case DL_Waveform_Float:
			
			//----------------------
			// process incoming data
			//----------------------
			
			nullChk( waveformData_float = calloc(nItegratedSamples, sizeof(float)) );
			
			// add data to the integration buffer
			memcpy(integrationBuffer + dev->AITaskSet->readAIData->nIntBuffElem[chIdx], AIReadBuffer + chIdx * nRead, nRead * sizeof(float64));
			dev->AITaskSet->readAIData->nIntBuffElem[chIdx] += nRead;
			nBufferSamples = dev->AITaskSet->readAIData->nIntBuffElem[chIdx];
			
			if (AIChSet->integrateFlag)
				// integrate
				for (i = 0; i < integration; i++) {
					k = i;
					for (j = 0; j < nItegratedSamples; j++) {
						waveformData_float[j] += (float) (integrationBuffer[k] * AIChSet->gain + AIChSet->offset);
						k += integration;
					}							   
				}
			else
				// jump over oversampled samples
				for (i = 0; i < nItegratedSamples; i++)
					waveformData_float[i] = (float) (integrationBuffer[i*integration] * AIChSet->gain + AIChSet->offset);
				
			
			// shift unprocessed samples from the end of the buffer to its beginning
			if (nRemainingSamples)
				memmove(integrationBuffer, integrationBuffer + (nBufferSamples - nRemainingSamples), nRemainingSamples * sizeof(float64));
			
			dev->AITaskSet->readAIData->nIntBuffElem[chIdx] = nRemainingSamples;
				
			//-------------------- 
			// prepare data packet
			//--------------------
			
			nullChk( waveform 	= init_Waveform_type(Waveform_Float, dev->AITaskSet->timing->sampleRate, nItegratedSamples, (void**)&waveformData_float) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_Float, &waveform,  NULL,(DiscardPacketDataFptr_type) discard_Waveform_type) );
				
			break;
		
		/*
		case DL_Waveform_UInt:
			
			nullChk( waveformData_uInt32 = calloc(nItegratedSamples, sizeof(uInt32)) );
			
			// calculate scaling factor to transform double to uInt32 given min & max values
			maxSignal = (double)UINT_MAX;
			scalingFactor = maxSignal/(AIChSet->ScaleMax - AIChSet->ScaleMin);
			
			
			for (int i = 0; i < nItegratedSamples; i++) {
				// scale data
				scaledSignal= scalingFactor * (*(AIReadBuffer + chIdx * nItegratedSamples + i) - AIChSet->ScaleMin);
				// apply limits and transform data
				if (scaledSignal < 0) waveformData_uInt32[i] = 0;
				else
					if (scaledSignal > maxSignal) waveformData_uInt32[i] = UINT_MAX;
					else
						waveformData_uInt32[i] = (uInt32) scaledSignal;
			}
				
			nullChk( waveform = init_Waveform_type(Waveform_UInt, dev->AITaskSet->timing->sampleRate/AIChSet->integrate, nItegratedSamples, (void**)&waveformData_uInt32) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UInt, &waveform,  NULL,(DiscardPacketDataFptr_type) discard_Waveform_type) );
				
			break;
				
		case DL_Waveform_UShort:
				
			// calculate scaling factor to transform double to uInt16 given min & max values
			maxSignal = (double)USHRT_MAX;
			scalingFactor = maxSignal/(AIChSet->ScaleMax - AIChSet->ScaleMin);
				
			nullChk( waveformData_uInt16 = calloc(nItegratedSamples, sizeof(uInt16)) );
				
			for (int i = 0; i < nItegratedSamples; i++) {
				// scale data
				scaledSignal= scalingFactor * (*(AIReadBuffer + chIdx * nItegratedSamples + i) - AIChSet->ScaleMin);
				// apply limits and transform data
				if (scaledSignal < 0) waveformData_uInt16[i] = 0;
				else
					if (scaledSignal > maxSignal) waveformData_uInt16[i] = USHRT_MAX;
					else
						waveformData_uInt16[i] = (uInt16) scaledSignal;
			}
				
			nullChk( waveform = init_Waveform_type(Waveform_UShort, dev->AITaskSet->timing->sampleRate/AIChSet->integrate, nItegratedSamples, (void**)&waveformData_uInt16) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UShort, &waveform,  NULL,(DiscardPacketDataFptr_type) discard_Waveform_type) );
				
			break;
				
		case DL_Waveform_UChar:
				
			// calculate scaling factor to transform double to uInt8 given min & max values
			maxSignal = (double)UCHAR_MAX;
			scalingFactor = maxSignal/(AIChSet->ScaleMax - AIChSet->ScaleMin);
				
			nullChk( waveformData_uInt8 = calloc(nItegratedSamples, sizeof(uInt8)) );
				
			for (int i = 0; i < nItegratedSamples; i++) {
				// scale data
				scaledSignal= scalingFactor * (*(AIReadBuffer + chIdx * nItegratedSamples + i) - AIChSet->ScaleMin);
				// apply limits and transform data
				if (scaledSignal < 0) waveformData_uInt8[i] = 0;
				else
					if (scaledSignal > maxSignal) waveformData_uInt8[i] = UCHAR_MAX;
					else
						waveformData_uInt8[i] = (uInt8) scaledSignal;
			}
				
			nullChk( waveform = init_Waveform_type(Waveform_UChar, dev->AITaskSet->timing->sampleRate/AIChSet->integrate, nItegratedSamples, (void**)&waveformData_uInt8) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UChar, &waveform,  NULL,(DiscardPacketDataFptr_type) discard_Waveform_type) );
				
			break;
		*/
		default:
			
			errMsg = StrDup("Must be implemented correctly still!"); 
			error = -1;
			goto Error;
					
			
			break;
	}

	//-------------------------------
	// send data packet with waveform
	//-------------------------------
	
	errChk( SendDataPacket(AIChSet->srcVChan, &dataPacket, 0, &errMsg) );
		
	return 0;
	
Error:
	
	// cleanup
	OKfree(waveformData_double);
	OKfree(waveformData_float);
	OKfree(waveformData_uInt8);
	OKfree(waveformData_uInt16);
	OKfree(waveformData_uInt32);
	discard_Waveform_type(&waveform);
	discard_DataPacket_type(&dataPacket);
	
	// no error message means out of memory
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	if (errorInfo)
		*errorInfo = FormatMsg(error, "SendAIBufferData", errMsg);
	OKfree(errMsg);
	
	return error;
}

int32 CVICALLBACK AIDAQmxTaskDataAvailable_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	Dev_type*			dev 					= callbackData;
	float64*    		readBuffer				= NULL;				// temporary buffer to place data into
	uInt32				nAI;
	int					nRead;
	int					error					= 0;
	char*				errMsg					= NULL;
	ChanSet_type**		chanSetPtr				= NULL;
	size_t				nChans					= ListNumItems(dev->AITaskSet->chanSet);
	size_t				chIdx					= 0;
	
	// allocate memory to read samples
	DAQmxErrChk( DAQmxGetTaskAttribute(taskHandle, DAQmx_Task_NumChans, &nAI) );
	nullChk( readBuffer = malloc(nSamples * nAI * sizeof(float64)) );
	
	// read samples from the AI buffer
	DAQmxErrChk( DAQmxReadAnalogF64(taskHandle, nSamples, dev->AITaskSet->timeout, DAQmx_Val_GroupByChannel , readBuffer, nSamples * nAI, &nRead, NULL) );
	
	for (size_t i = 1; i <= nChans; i++) {
		chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		// include only channels for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		// forward data from the AI buffer to the VChan if the task was not aborted	
		if (!GetTaskControlAbortIterationFlag(dev->taskController))
			errChk( SendAIBufferData(dev, *chanSetPtr, chIdx, nRead, readBuffer, &errMsg) ); 
		// next AI channel
		chIdx++;
	}
	
	// cleanup
	OKfree(readBuffer);
	
	return 0;
	
DAQmxError:
	
	int errMsgBuffSize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((errMsgBuffSize+1) * sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, errMsgBuffSize+1);
	
Error:
	
	if (!errMsg)
		errMsg = FormatMsg(error, "AIDAQmxTaskDataAvailable_CB", "Out of memory");
	
	//-------------------------
	// cleanup
	//-------------------------
	// clear all device tasks
	ClearDAQmxTasks(dev); 
	
	OKfree(readBuffer); 

	// terminate task controller iteration
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	OKfree(errMsg);
	
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition or generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK AIDAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	Dev_type*			dev 					= callbackData;
	uInt32				nSamples				= 0;						// number of samples per channel in the AI buffer
	float64*    		readBuffer				= NULL;						// temporary buffer to place data into       
	uInt32				nAI						= 0;
	int					error					= 0;
	char*				errMsg					= NULL;
	int					nRead					= 0;
	ChanSet_type**		chanSetPtr				= NULL;
	size_t				nChans					= ListNumItems(dev->AITaskSet->chanSet);
	int*				nActiveTasksPtr			= NULL;
	DataPacket_type*	nullPacket				= NULL;
	size_t				chIdx					= 0;
	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	if (status < 0) goto DAQmxError;
	
	// get all read samples from the input buffer 
	DAQmxErrChk( DAQmxGetReadAttribute(taskHandle, DAQmx_Read_AvailSampPerChan, &nSamples) ); 
	
	// if there are no samples left in the buffer, send NULL data packet and stop here, otherwise read them out
	if (!nSamples) {
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			// include only channels for which HW-timing is required
			if ((*chanSetPtr)->onDemand) continue;
			
			// send NULL packet to signal end of data transmission
			errChk( SendDataPacket((*chanSetPtr)->srcVChan, &nullPacket, 0, &errMsg) );
		}
		// stop the Task
		DAQmxErrChk( DAQmxTaskControl(taskHandle, DAQmx_Val_Task_Stop) );
		
		// Task Controller iteration is complete if all DAQmx Tasks are complete
		CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
		(*nActiveTasksPtr)--;
		
		if (!*nActiveTasksPtr)
			TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
		CmtReleaseTSVPtr(dev->nActiveTasks);
	
		return 0;
	}
	
	// allocate memory for samples
	DAQmxErrChk( DAQmxGetTaskAttribute(taskHandle, DAQmx_Task_NumChans, &nAI) );
	nullChk( readBuffer = malloc(nSamples * nAI * sizeof(float64)) );
	
	// read remaining samples from the AI buffer
	DAQmxErrChk( DAQmxReadAnalogF64(taskHandle, -1, dev->AITaskSet->timeout, DAQmx_Val_GroupByChannel , readBuffer, nSamples * nAI, &nRead, NULL) );
	
	for (size_t i = 1; i <= nChans; i++) {
		chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		// include only channels for which HW-timing is required
		if ((*chanSetPtr)->onDemand) continue;
		// forward data from AI buffer to the VChan 
		errChk( SendAIBufferData(dev, *chanSetPtr, chIdx, nRead, readBuffer, &errMsg) ); 
		// send NULL packet to signal end of data transmission
		errChk( SendDataPacket((*chanSetPtr)->srcVChan, &nullPacket, 0, &errMsg) );
		// next AI channel
		chIdx++;
	}
	
	// stop the Task
	DAQmxErrChk( DAQmxTaskControl(taskHandle, DAQmx_Val_Task_Stop) );
		
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
	(*nActiveTasksPtr)--;
		
	if (!*nActiveTasksPtr)
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
	CmtReleaseTSVPtr(dev->nActiveTasks);
	
	OKfree(readBuffer);
	return 0;
	
DAQmxError:
	
	int errMsgBuffSize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((errMsgBuffSize+1) * sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, errMsgBuffSize+1);
	
Error:
	
	if (!errMsg)
		errMsg = FormatMsg(error, "AIDAQmxTaskDone_CB", "Out of memory");
	
	//-------------------------
	// cleanup
	//-------------------------
	// clear all device tasks
	ClearDAQmxTasks(dev); 
	
	OKfree(readBuffer); 

	// terminate task controller iteration
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	OKfree(errMsg);
	
	return 0;
}

int32 CVICALLBACK AODAQmxTaskDataRequest_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	Dev_type*			dev 					= callbackData;
	int					error					= 0;
	char*				errMsg					= NULL;
	bool32				taskDoneFlag			= FALSE;
	
	// this is called back every number of samples that is set

	DAQmxIsTaskDone(dev->AOTaskSet->taskHndl, &taskDoneFlag);
	if (!taskDoneFlag) 
		errChk( WriteAODAQmx(dev, &errMsg) );
		
	return 0;
	
Error:
	
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	OKfree(errMsg);
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition of generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK AODAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	Dev_type*	dev 			= callbackData;
	int			error			= 0;
	int*		nActiveTasksPtr;
	
	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	DAQmxErrChk(status);
	
	// stop task explicitly
	DAQmxErrChk(DAQmxStopTask(taskHandle));
	// clear and init writeAOData used for continuous streaming
	//discard_WriteAOData_type(&dev->AOTaskSet->writeAOData);
	//dev->AOTaskSet->writeAOData = init_WriteAOData_type(dev);
	//if (!dev->AOTaskSet->writeAOData) goto MemError;
	
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
	(*nActiveTasksPtr)--;
		
	if (!*nActiveTasksPtr)
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
	CmtReleaseTSVPtr(dev->nActiveTasks);
		
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
	
	TaskControlIterationDone(dev->taskController, -1, "Out of memory", FALSE);
	ClearDAQmxTasks(dev); 
	return 0;
}

int32 CVICALLBACK DIDAQmxTaskDataAvailable_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	//Dev_type*	dev = callbackData;
	
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition of generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK DIDAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	//Dev_type*	dev = callbackData;
	
}

int32 CVICALLBACK DODAQmxTaskDataRequest_CB	(TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
	//Dev_type*	dev = callbackData;
	
	
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition of generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK DODAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	//Dev_type*	dev = callbackData;
	
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition or generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK CODAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
	Dev_type*			dev 			= callbackData;
	uInt32				nAI;
	int					error			= 0;
	int					nRead;
	ChanSet_type**		chanSetPtr;
	int*				nActiveTasksPtr	= NULL;
	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	if (status < 0) goto DAQmxError;
	
	// stop the Task
	DAQmxErrChk( DAQmxTaskControl(taskHandle, DAQmx_Val_Task_Stop) );
		
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
	(*nActiveTasksPtr)--;
		
	if (!*nActiveTasksPtr)
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
	CmtReleaseTSVPtr(dev->nActiveTasks);
	
	return 0;
	
DAQmxError:
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	char* errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	TaskControlIterationDone(dev->taskController, error, errMsg, FALSE);
	OKfree(errMsg);
	ClearDAQmxTasks(dev); 
	return 0;
	

}

//--------------------------------------------------------------------------------------------
// DAQmx module and VChan data exchange
//--------------------------------------------------------------------------------------------

/// HIFN Writes writeblock number of samples to the AO task.
static int WriteAODAQmx (Dev_type* dev, char** errorInfo) 
{
#define	WriteAODAQmx_Err_WaitingForDataTimeout		-1
#define WriteAODAQmx_Err_DataTypeNotSupported		-2
#define WriteAODAQmx_Err_NULLReceivedBeforeData		-3
	
	DataPacket_type* 		dataPacket									= NULL;
	DLDataTypes				dataPacketType								= 0;
	void*					dataPacketData								= NULL;
	double*					waveformData								= NULL;
	float*					floatWaveformData							= NULL;
	WriteAOData_type*    	data            							= dev->AOTaskSet->writeAOData;
	size_t          		queue_items									= 0;
	size_t          		ncopies										= 0;	// number of datapacket copies to fill at least a writeblock size
	double					nRepeats									= 1;
	int             		error           							= 0;
	char*					errMsg										= NULL;
	float64*        		tmpbuff										= NULL;
	CmtTSQHandle			tsqID										= 0;
	char* 					DAQmxErrMsg									= NULL;
	char					CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE]	= "";
	int 					buffsize									= 0;
	int						itemsRead									= 0;
	int						nSamplesWritten								= 0;
	BOOL					stopAOTaskFlag								= TRUE;

	// cycle over channels
	for (int i = 0; i < data->numchan; i++) {
		tsqID = GetSinkVChanTSQHndl(data->sinkVChans[i]);
		while (data->databuff_size[i] < data->writeblock) {
			
			// if datain[i] is empty, get data packet from queue if NULL packet was not yet received
			if (!data->datain_size[i]) {
				
				if (!data->nullPacketReceived[i])
					CmtErrChk( itemsRead = CmtReadTSQData (tsqID, &dataPacket, 1, GetSinkVChanReadTimeout(data->sinkVChans[i]), 0) );
				
				// process NULL packet
				if (!dataPacket) {
					data->nullPacketReceived[i] = TRUE;
																			
					if (!data->databuff_size[i]) {
						// if NULL received and there is no data in the AO buffer, stop AO task if running
						int*		nActiveTasksPtr	= NULL;
					
						DAQmxErrChk( DAQmxStopTask(dev->AOTaskSet->taskHndl) );
						// Task Controller iteration is complete if all DAQmx Tasks are complete
						CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
						(*nActiveTasksPtr)--;
		
						if (!*nActiveTasksPtr)
						TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
						CmtReleaseTSVPtr(dev->nActiveTasks);
						
						return 0;
						
					} else {
						
						// repeat last value from the buffer until all AO channels have received a NULL packet
						data->datain[i] = malloc (data->writeblock * sizeof(float64));
						for (int j = 0; j < data->writeblock ; j++)
							data->datain[i][j] = data->databuff[i][data->databuff_size[i] - 1];
						
						data->datain_size[i]		= data->writeblock;
						data->datain_repeat[i] 		= 0;
						data->datain_remainder[i] 	= 0;
						data->datain_loop[i] 		= TRUE;
						
						goto SkipPacket;
					}
				}
					
				// if timeout occured and no data packet was read, generate error
				if (!itemsRead) {
					error = WriteAODAQmx_Err_WaitingForDataTimeout;
					errMsg = FormatMsg(error, "WriteAODAQmx", "Waiting for AO data timed out");
					goto Error;
				}
				
				// copy data packet to datain
				dataPacketData = GetDataPacketPtrToData (dataPacket, &dataPacketType);
				switch (dataPacketType) {
					case DL_Waveform_Double:
						waveformData = *(double**)GetWaveformPtrToData(*(Waveform_type**)dataPacketData, &data->datain_size[i]);
						data->datain[i] = malloc (data->datain_size[i] * sizeof(float64));
						memcpy(data->datain[i], waveformData, data->datain_size[i] * sizeof(float64));
						nRepeats = 1;
						break;
						
					case DL_Waveform_Float:
						floatWaveformData = *(float**)GetWaveformPtrToData(*(Waveform_type**)dataPacketData, &data->datain_size[i]);
						data->datain[i] = malloc (data->datain_size[i] * sizeof(float64));
						// transform float data to double data
						for (size_t j = 0; j < data->datain_size[i]; j++)
							data->datain[i][j] = (double) floatWaveformData[j];
						nRepeats = 1;
						break;
						
					case DL_RepeatedWaveform_Double:
						waveformData = *(double**)GetRepeatedWaveformPtrToData(*(RepeatedWaveform_type**)dataPacketData, &data->datain_size[i]);
						data->datain[i] = malloc (data->datain_size[i] * sizeof(float64));
						memcpy(data->datain[i], waveformData, data->datain_size[i] * sizeof(float64));
						nRepeats = GetRepeatedWaveformRepeats(*(RepeatedWaveform_type**)dataPacketData);
						break;
					
					case DL_RepeatedWaveform_Float:
						floatWaveformData = *(float**)GetRepeatedWaveformPtrToData(*(RepeatedWaveform_type**)dataPacketData, &data->datain_size[i]);
						data->datain[i] = malloc (data->datain_size[i] * sizeof(float64));
						// transform float data to double data
						for (size_t j = 0; j < data->datain_size[i]; j++)
							data->datain[i][j] = (double) floatWaveformData[j];
						nRepeats = GetRepeatedWaveformRepeats(*(RepeatedWaveform_type**)dataPacketData);
						break;
						
					default:
						error = WriteAODAQmx_Err_DataTypeNotSupported;
						errMsg = FormatMsg(error, "WriteAODAQmx", "Data type not supported");
						goto Error;
				}
				
				
				// copy repeats
				if (nRepeats) {
					data->datain_repeat[i]    = (size_t) nRepeats;
					data->datain_remainder[i] = (size_t) ((nRepeats - (double) data->datain_repeat[i]) * (double) data->datain_size[i]);
					data->datain_loop[i]      = FALSE;
				} else data->datain_loop[i]   = TRUE;
				
				// data packet not needed anymore, release it (data is deleted if there are no other sinks that need it)
				ReleaseDataPacket(&dataPacket); 
			}
			
SkipPacket:
			
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
					CmtErrChk ( CmtGetTSQAttribute(tsqID, ATTR_TSQ_ITEMS_IN_QUEUE, &queue_items) );
					
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
	
	// build dataout from buffers and at the same time, update elements left in the buffers
	for (size_t i = 0; i < data->numchan; i++) {
		// build dataout with one writeblock from databuff
		memcpy(data->dataout + i * data->writeblock, data->databuff[i], data->writeblock * sizeof(float64));
		// keep remaining data
		tmpbuff = malloc ((data->databuff_size[i] - data->writeblock) * sizeof(float64));
		memcpy(tmpbuff, data->databuff[i] + data->writeblock, (data->databuff_size[i] - data->writeblock) * sizeof(float64)); 
		// clean buffer and restore remaining data
		OKfree(data->databuff[i]);
		data->databuff[i] = tmpbuff;
		// update number of elements in databuff[i]
		data->databuff_size[i] -= data->writeblock;
	}
	
	// if all AO channels received a NULL packet, stop AO task
	for (int j = 0; j < data->numchan; j++)
		if (!data->nullPacketReceived[j]) {
			stopAOTaskFlag = FALSE;
			break;
		}
	
	DAQmxErrChk(DAQmxWriteAnalogF64(dev->AOTaskSet->taskHndl, data->writeblock, 0, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, data->dataout, &nSamplesWritten, NULL));
		
	if (stopAOTaskFlag && (!data->writeBlocksLeftToWrite--)) {
		int*	nActiveTasksPtr = NULL;
		
		DAQmxErrChk( DAQmxStopTask(dev->AOTaskSet->taskHndl) );
		// Task Controller iteration is complete if all DAQmx Tasks are complete
		CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
		(*nActiveTasksPtr)--;
		
		if (!*nActiveTasksPtr)
		TaskControlIterationDone(dev->taskController, 0, "", FALSE);
		
		CmtReleaseTSVPtr(dev->nActiveTasks);
		
	}
	
	return 0; // no error
	
DAQmxError:
	
	buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	DAQmxErrMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(DAQmxErrMsg, buffsize+1);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "WriteAODAQmx", DAQmxErrMsg);
	OKfree(DAQmxErrMsg);
	ReleaseDataPacket(&dataPacket);
	return error;
	
CmtError:
	
	CmtGetErrorMessage (error, CmtErrMsgBuffer);
	if (errorInfo)
		*errorInfo = FormatMsg(error, "WriteAODAQmx", CmtErrMsgBuffer); 
	ReleaseDataPacket(&dataPacket);
	return error;
	
Error:
	
	if (errorInfo)
		*errorInfo = errMsg;
	else
		OKfree(errMsg);
	
	ReleaseDataPacket(&dataPacket);
	return error;
}

/*
/// HIFN Writes writeblock number of samples to the DO task.
static int WriteDODAQmx(Dev_type* dev, char** errorInfo) 
{
#define	WriteDODAQmx_Err_WaitingForDataTimeout		-1
#define WriteDODAQmx_Err_DataTypeNotSupported		-2

	DataPacket_type* 		dataPacket;
	DLDataTypes				dataPacketType;
	void*					dataPacketData;
	double*					waveformData								= NULL;
	WriteDOData_type*    	data            							= (WriteDOData_type*) dev->DOTaskSet->writeDOData;
	size_t          		queue_items;
	size_t          		ncopies;                								// number of datapacket copies to fill at least a writeblock size
	int						itemsRead;
	size_t         		 	numwrote; 
	double					nRepeats									= 1;
	int             		error           							= 0;
	uInt32*        			tmpbuff;
	FCallReturn_type*		fCallReturn									= NULL;
	BOOL					writeBlockFilledFlag						= TRUE;  	// assume that each output channel has at least data->writeblock elements 
	size_t					nSamples;
	CmtTSQHandle			tsqID;
	char* 					DAQmxErrMsg									= NULL;
	char					CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE];
	int 					buffsize;
	
	// cycle over channels
	for (int i = 0; i < data->numchan; i++) {
		tsqID = GetSinkVChanTSQHndl(data->sinkVChans[i]);
		while (data->databuff_size[i] < data->writeblock) {
			
			// if datain[i] is empty, get data packet from queue
			if (!data->datain_size[i]) {
				
				// try to get a non-NULL data packet from queue 
				do { 
					CmtErrChk ( itemsRead = CmtReadTSQData (tsqID, &dataPacket, 1, 0, 0) );
				} while (!dataPacket || !itemsRead);
				
				// if there are no more data packets, then skip this channel
				if (!itemsRead) {
					writeBlockFilledFlag = FALSE;				// there is not enough data for this channel 
					break;
				}
				
				// copy data packet to datain
				dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType);
				switch (dataPacketType) {
						//
					case DL_Waveform_Char:						
					case DL_Waveform_UChar:						
					case DL_Waveform_Short:						
					case DL_Waveform_UShort:					
					case DL_Waveform_Int:						
					case DL_Waveform_UInt:	
						waveformData = GetWaveformPtrToData(dataPacketData, &data->datain_size[i]);  // wrong <-------------------------
						nRepeats = 1;
						break;
						
					case DL_RepeatedWaveform_Char:						
					case DL_RepeatedWaveform_UChar:						
					case DL_RepeatedWaveform_Short:						
					case DL_RepeatedWaveform_UShort:					
					case DL_RepeatedWaveform_Int:						
					case DL_RepeatedWaveform_UInt:
						waveformData = GetRepeatedWaveformPtrToData(dataPacketData, &data->datain_size[i]);  // wrong <---------------------------
						nRepeats = GetRepeatedWaveformRepeats(dataPacketData);
						break;
				}
				data->datain[i] = malloc (data->datain_size[i] * sizeof(uInt32));
				memcpy(data->datain[i], waveformData, data->datain_size[i] * sizeof(uInt32));
				
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
				data->databuff[i] = realloc (data->databuff[i], (data->databuff_size[i] + ncopies * data->datain_size[i]) * sizeof(uInt32));
				
				// insert one or more data copies in the buffer
				for (int j = 0; j < ncopies; j++)
					memcpy(data->databuff[i] + data->databuff_size[i] + j * data->datain_size[i], data->datain[i], data->datain_size[i] * sizeof (uInt32));
				
				// update number of data elements in the buffer
				data->databuff_size[i] += ncopies * data->datain_size[i];
				
				// if repeats is finite,  update number of repeats left in datain
				if (!data->datain_loop[i]) 
					data->datain_repeat[i] -= ncopies;
				else {
					// if repeats is infinite, check if there is another data packet waiting in the queue
					CmtErrChk( CmtGetTSQAttribute(tsqID, ATTR_TSQ_ITEMS_IN_QUEUE, &queue_items) );
					
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
					data->databuff[i] = realloc (data->databuff[i], (data->databuff_size[i] + data->datain_repeat[i] * data->datain_size[i]) * sizeof(uInt32));
					
					// insert the datain[i] in the buffer repeat times
					for (int j = 0; j < data->datain_repeat[i]; j++)
						memcpy(data->databuff[i] + data->databuff_size[i] + j * data->datain_size[i], data->datain[i], data->datain_size[i] * sizeof (uInt32));
			
					// update number of data elements in the buffer
					data->databuff_size[i] += data->datain_repeat[i] * data->datain_size[i];
					
					// no more repeats left
					data->datain_repeat[i] = 0;
				
				} else {
					
					// copy remainding elements if there are any
					if (data->datain_remainder[i]){
						data->databuff[i] = realloc (data->databuff[i], (data->databuff_size[i] + data->datain_remainder[i]) * sizeof(uInt32));
						memcpy(data->databuff[i] + data->databuff_size[i], data->datain[i], data->datain_remainder[i] * sizeof(uInt32));
						data->databuff_size[i] += data->datain_remainder[i];
						data->datain_remainder[i] = 0;
					}
					
					// datain[i] not needed anymore
					OKfree(data->datain[i]);
					data->datain_size[i] = 0;
				
				}
			
		}
	}
	
	
	if (writeBlockFilledFlag) nSamples = data->writeblock;
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
		memcpy(data->dataout + i * nSamples, data->databuff[i], nSamples * sizeof(uInt32));
		// keep remaining data
		tmpbuff = malloc ((data->databuff_size[i] - nSamples) * sizeof(uInt32));
		memcpy(tmpbuff, data->databuff[i] + nSamples, (data->databuff_size[i] - nSamples) * sizeof(uInt32)); 
		// clean buffer and restore remaining data
		OKfree(data->databuff[i]);
		data->databuff[i] = tmpbuff;
		// update number of elements in databuff[i]
		data->databuff_size[i] -= nSamples;
	}
	
	DAQmxErrChk(DAQmxWriteDigitalU32 (dev->DOTaskSet->taskHndl, nSamples,0, dev->DOTaskSet->timeout, DAQmx_Val_GroupByChannel, data->dataout, &numwrote, NULL));
	 
	return NULL; 
	
DAQmxError:
	
	buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	DAQmxErrMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(DAQmxErrMsg, buffsize+1);
	fCallReturn = init_FCallReturn_type(error, "WriteDODAQmx", DAQmxErrMsg);
	free(DAQmxErrMsg);
	return fCallReturn;
	
CmtError:
	
	CmtGetErrorMessage (error, CmtErrMsgBuffer);
	fCallReturn = init_FCallReturn_type(error, "WriteDODAQmx", CmtErrMsgBuffer);   
	ClearDAQmxTasks(dev);
	return fCallReturn;
}

*/

//--------------------------------------------
// Various DAQmx module managemement functions
//--------------------------------------------

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

//------------------------------------------------------------------------------------------------------------
// DAQmx device management
//------------------------------------------------------------------------------------------------------------

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
 
/*
char* DAQNameCopy(char* oldname)
{
	 char* 	newname	= NULL;
	 size_t i;
	 size_t j		= 0;
	 size_t len		= strlen(oldname);
	 
	 newname = malloc(len+1); 
	 for(i = 0; i <= len; i++){
		newname[j] = oldname[i];
		if ((oldname[i] == '\\') && (oldname[i+1] != '\\')){   
			newname = realloc(newname, len+1);
			newname[j+1]='\\';
		}
		j++;
	 }
	 
	 return newname;
}
*/

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
					if (!(newDAQmxDev = init_Dev_type(nidaq, newDAQmxDevAttrPtr, newTCName))) {
						discard_DevAttr_type(&newDAQmxDevAttrPtr);
						OKfree(newTCName);
						DLMsg(OutOfMemoryMsg, 1);
						return 0;
					}
					
					ListInsertItem(nidaq->DAQmxDevices, &newDAQmxDev, END_OF_LIST);
					DLAddTaskController((DAQLabModule_type*)nidaq, newDAQmxDev->taskController);
					
					// copy DAQmx Task settings panel to module tab and get handle to the panel inserted in the tab
					newTabPageIdx = InsertPanelAsTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, -1, nidaq->taskSetPanHndl);
					GetPanelHandleFromTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, newTabPageIdx, &newDAQmxDevPanHndl);
					// keep track of the DAQmx task settings panel handle
					newDAQmxDev->devPanHndl = newDAQmxDevPanHndl;
					// change tab title to new Task Controller name
					//newTabName = DAQNameCopy(newDAQmxDevAttrPtr->name);
					newTabName = StrDup(newDAQmxDevAttrPtr->name);
					
					AppendString(&newTabName, ": ", -1);
					AppendString(&newTabName, newTCName, -1);
					SetTabPageAttribute(nidaq->mainPanHndl, NIDAQmxPan_Devices, newTabPageIdx, ATTR_LABEL_TEXT, newTabName);
					OKfree(newTabName);
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
					
					//cleanup test lex
					//discard_DevAttr_type(&newDAQmxDevAttrPtr);
					//discard_Dev_type(&newDAQmxDev);
					
					
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

static void CVICALLBACK DeleteDev_CB (int menuBarHandle, int menuItemID, void *callbackData, int panelHandle)
{
	NIDAQmxManager_type* 	nidaq 			= (NIDAQmxManager_type*) callbackData;
	Dev_type**				DAQmxDevPtr		= NULL;
	int						activeTabIdx;		// 0-based index
	int						nTabPages;

	// get selected tab index
	GetActiveTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, &activeTabIdx);
	// get pointer to selected DAQmx device and remove its Task Controller from the framework
	DAQmxDevPtr = ListGetPtrToItem(nidaq->DAQmxDevices, activeTabIdx + 1);
	
	// unregister device from framework
	UnregisterDeviceFromFramework(*DAQmxDevPtr);
	
	// discard DAQmx device data
	discard_Dev_type(DAQmxDevPtr);
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

static void UnregisterDeviceFromFramework (Dev_type* dev)
{
	ChanSet_type**			chanSetPtr		= NULL;
	size_t					nChans			= 0;
	
	//--------------------------------------------
	// Remove task controller from framework
	//--------------------------------------------
	
	DLRemoveTaskController((DAQLabModule_type*)dev->niDAQModule, dev->taskController);
	
	//--------------------------------------------
	// Remove HW triggers and VChans from framework
	//--------------------------------------------
	
	//-----------
	// AI Task
	//-----------
	
	if (dev->AITaskSet) {
		// n samples Sink VChan   
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AITaskSet->timing->nSamplesSinkVChan);
		// n samples Source VChan
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AITaskSet->timing->nSamplesSourceVChan);
		// sampling rate Sink VChan
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AITaskSet->timing->samplingRateSinkVChan);
		// sampling rate Source VChan
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AITaskSet->timing->samplingRateSourceVChan);
		// HW triggers
		DLUnregisterHWTrigMaster(dev->AITaskSet->HWTrigMaster);
		DLUnregisterHWTrigSlave(dev->AITaskSet->HWTrigSlave);
		// AI channels
		nChans = ListNumItems(dev->AITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) (*chanSetPtr)->srcVChan);
		}
	}
	
	//-----------
	// AO Task
	//-----------
	
	if (dev->AOTaskSet) {
		// n samples Sink VChan
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AOTaskSet->timing->nSamplesSinkVChan);
		// n samples Source VChan
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AOTaskSet->timing->nSamplesSourceVChan);
		// sampling rate Sink VChan
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AOTaskSet->timing->samplingRateSinkVChan);
		// sampling rate Source VChan
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) dev->AOTaskSet->timing->samplingRateSourceVChan);
		// HW triggers
		DLUnregisterHWTrigMaster(dev->AOTaskSet->HWTrigMaster);
		DLUnregisterHWTrigSlave(dev->AOTaskSet->HWTrigSlave);
		// AO channels
		nChans = ListNumItems(dev->AOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule , (VChan_type*) (*chanSetPtr)->sinkVChan);
		}
	}
	
	//-----------
	// DI Task
	//-----------
	
	if (dev->DITaskSet) {
		// n samples
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule , (VChan_type*) dev->DITaskSet->timing->nSamplesSinkVChan);
		// sampling rate
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule , (VChan_type*) dev->DITaskSet->timing->samplingRateSinkVChan);
		// DI channels
		nChans = ListNumItems(dev->DITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule , (VChan_type*) (*chanSetPtr)->srcVChan);
		}
	}
	
	//-----------
	// DO Task
	//-----------
	
	if (dev->DOTaskSet) {
		// n samples
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule , (VChan_type*) dev->DOTaskSet->timing->nSamplesSinkVChan);
		// sampling rate
		DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule , (VChan_type*) dev->DOTaskSet->timing->samplingRateSinkVChan);
		// DO channels
		nChans = ListNumItems(dev->DOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) (*chanSetPtr)->sinkVChan);
		}
	}
	
	//----------- 
	// CI Task
	//-----------
	
	if (dev->CITaskSet) {
		// CI channels
		nChans = ListNumItems(dev->CITaskSet->chanTaskSet); 
		for (size_t i = 1; i <= nChans; i++) {  
			chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		
			if ((*chanSetPtr)->srcVChan)
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) (*chanSetPtr)->srcVChan);   
		
			if ((*chanSetPtr)->sinkVChan) 
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) (*chanSetPtr)->sinkVChan);   
		}
	}
	
	//----------- 
	// CO Task
	//-----------
	
	if (dev->COTaskSet) {
		// CO channels
		nChans = ListNumItems(dev->COTaskSet->chanTaskSet); 
		for (size_t i = 1; i <= nChans; i++) {  
			chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		
			// Sink & Source VChans used to exchange pulse train info
			if ((*chanSetPtr)->srcVChan)
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) (*chanSetPtr)->srcVChan);   
		
			if ((*chanSetPtr)->sinkVChan) 
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) (*chanSetPtr)->sinkVChan);
			
			// HW triggers
			if ((*chanSetPtr)->HWTrigMaster)
				DLUnregisterHWTrigMaster((*chanSetPtr)->HWTrigMaster);
			
			if ((*chanSetPtr)->HWTrigSlave)
				DLUnregisterHWTrigSlave((*chanSetPtr)->HWTrigSlave);
			
			
		}
	}
	
}

//---------------------------------------------------------------------------------------------------------------------------
// Trigger settings callbacks
//---------------------------------------------------------------------------------------------------------------------------

static int CVICALLBACK TaskStartTrigType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;
	char*				errMsg			= NULL;
	int					error			= 0;
	int 				trigtype;
	
	if (event != EVENT_COMMIT) return 0; // stop here if there are other events
	
	// get trigger type
	GetCtrlVal(panel, control,  &trigtype);
	taskTrigPtr->trigType = trigtype;
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
			
	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );             
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TaskReferenceTrigType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;
	char*				errMsg			= NULL;
	int					error			= 0;
	int					trigType;

	if (event != EVENT_COMMIT) return 0; // stop here if event is not commit			
		 				
			
	// get trigger type
			
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
			SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_UNSIGNED_INTEGER);
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
			SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_UNSIGNED_INTEGER);
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
			SetCtrlAttribute(panel, taskTrigPtr->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_UNSIGNED_INTEGER);
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
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );            
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TriggerSlope_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events
		
	GetCtrlVal(panel, control, &taskTrigPtr->slope);

	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );    
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
	
}

static int CVICALLBACK TriggerLevel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events
	
	GetCtrlVal(panel, control, &taskTrigPtr->level);
	
	// configure device
	errChk( ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TriggerSource_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	int			 		buffsize		= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events 
	
	GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
	OKfree(taskTrigPtr->trigSource);
	taskTrigPtr->trigSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null  
	GetCtrlVal(panel, control, taskTrigPtr->trigSource);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TriggerWindowType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events    
  
	GetCtrlVal(panel, control, &taskTrigPtr->wndTrigCond);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TriggerWindowBttm_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events  
	
	GetCtrlVal(panel, control, &taskTrigPtr->wndBttm);
	
	// configure device
	errChk( ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );

	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TriggerWindowTop_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{ 
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events 

	GetCtrlVal(panel, control, &taskTrigPtr->wndTop);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TriggerPreTrigDuration_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	double				duration;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events  
	
	GetCtrlVal(panel, control, &duration);
	// calculate number of samples given sampling rate
	taskTrigPtr->nPreTrigSamples = (uInt32) (*taskTrigPtr->samplingRate * duration);
	SetCtrlVal(panel, taskTrigPtr->preTrigNSamplesCtrlID, taskTrigPtr->nPreTrigSamples);
	// recalculate duration to match the number of samples and sampling rate
	SetCtrlVal(panel, control, taskTrigPtr->nPreTrigSamples/(*taskTrigPtr->samplingRate)); 
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;
}

static int CVICALLBACK TriggerPreTrigSamples_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	TaskTrig_type* 		taskTrigPtr 	= callbackData;    
	char*				errMsg			= NULL;
	int					error			= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events    
	
	GetCtrlVal(panel, control, &taskTrigPtr->nPreTrigSamples);
	// recalculate duration
	SetCtrlVal(panel, taskTrigPtr->preTrigDurationCtrlID, taskTrigPtr->nPreTrigSamples/(*taskTrigPtr->samplingRate)); 
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrigPtr->device, &errMsg) );
	
	return 0;
	
Error:
	
	DLMsg(errMsg, 1);
	OKfree(errMsg);
	
	return 0;

}

//-----------------------------------------------------------------------------------------------------
// VChan Callbacks
//-----------------------------------------------------------------------------------------------------

static void ADNSamplesSinkVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	ADTaskSet_type* 	tskSet	= VChanOwner;
	
	// dim number of samples controls and duration
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DIMMED, 1);
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Duration, ATTR_DIMMED, 1);
	
}

static void	ADNSamplesSinkVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	ADTaskSet_type* 	tskSet	= VChanOwner;
	
	// undim number of samples controls and duration
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DIMMED, 0);
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Duration, ATTR_DIMMED, 0);
	
}

static void	ADNSamplesSourceVChan_Connected	(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	ADTaskSet_type* 	tskSet			= VChanOwner; 
	uInt64*				nSamplesPtr		= NULL;
	DataPacket_type*	dataPacket		= NULL;
	int					error			= 0;
	char*				errMsg			= NULL;
	
	// send number of samples if task is finite
	if (tskSet->timing->measMode != MeasFinite) return;
	
	nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
	*nSamplesPtr = tskSet->timing->nSamples;
	nullChk( dataPacket = init_DataPacket_type(DL_ULongLong, &nSamplesPtr, NULL,NULL) );
	errChk( SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errMsg) );
	
	return;
	
Error:
	
	OKfree(nSamplesPtr);
	discard_DataPacket_type(&dataPacket);
	if (errMsg) {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	} else
		DLMsg("Out of memory", 1);
}

static void ADSamplingRateSinkVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	ADTaskSet_type* 	tskSet			= VChanOwner;
	
	// dim sampling rate control
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_SamplingRate, ATTR_DIMMED, 1);
	
}

static void ADSamplingRateSinkVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	ADTaskSet_type* 	tskSet			= VChanOwner;
	
	// undim sampling rate control
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_SamplingRate, ATTR_DIMMED, 0);
}

static void	ADSamplingRateSourceVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	ADTaskSet_type* 	tskSet			= VChanOwner;
	double*				samplingRatePtr	= NULL;
	DataPacket_type*	dataPacket		= NULL;
	int					error			= 0;
	char*				errMsg			= NULL;
	
	
	nullChk( samplingRatePtr = malloc(sizeof(double)) );
	*samplingRatePtr = tskSet->timing->sampleRate;
	nullChk( dataPacket = init_DataPacket_type(DL_Double, &samplingRatePtr, NULL,NULL) );
	errChk( SendDataPacket(tskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errMsg) );
	
	return;
	
Error:
	
	OKfree(samplingRatePtr);
	discard_DataPacket_type(&dataPacket);
	if (errMsg) {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	} else
		DLMsg("Out of memory", 1);
	
}

static void COPulseTrainSinkVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	ChanSet_CO_type* 		COChanSet		= GetVChanOwner(self);
	int						timingPanHndl;
	
	// dim timing UI pulse train controls
	GetPanelHandleFromTabPage(COChanSet->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl);
	SetPanelAttribute(timingPanHndl, ATTR_DIMMED, 1);
	
}

static void COPulseTrainSinkVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	ChanSet_CO_type* 		COChanSet		= GetVChanOwner(self);
	int						timingPanHndl;
	
	// undim timing UI pulse train controls
	GetPanelHandleFromTabPage(COChanSet->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl);
	SetPanelAttribute(timingPanHndl, ATTR_DIMMED, 0);
}

// pulsetrain command VChan connected callback
static void	COPulseTrainSourceVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	// construct pulse train packet when a connection is created with default settings and send it
	ChanSet_CO_type* 		COChanSet		= GetVChanOwner(self);	  
	PulseTrain_type* 		pulseTrain		= NULL;
	DataPacket_type*  		dataPacket		= NULL;
	int						error			= 0;
	char*					errMsg			= NULL;
	PulseTrainTimingTypes 	pulseType; 
	
	nullChk ( pulseTrain = CopyPulseTrain(COChanSet->pulseTrain) );
	pulseType = GetPulseTrainType(pulseTrain); 
	
	switch(pulseType){
			
		case PulseTrain_Freq:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, &pulseTrain, NULL,(DiscardPacketDataFptr_type) discard_PulseTrain_type) );  
			break;
			
		case PulseTrain_Time:
			nullChk ( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, &pulseTrain, NULL,(DiscardPacketDataFptr_type) discard_PulseTrain_type) );  
			break;
			
		case PulseTrain_Ticks: 
			nullChk ( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, &pulseTrain, NULL,(DiscardPacketDataFptr_type) discard_PulseTrain_type) );  
			break;
			
	}
	
	// send data packet with pulsetrain
	errChk( SendDataPacket((SourceVChan_type*)self, &dataPacket, 0, &errMsg) );

	return;
Error:
	
	discard_PulseTrain_type(&pulseTrain);
	discard_DataPacket_type(&dataPacket);
	
	discard_DataPacket_type(&dataPacket);
	if (errMsg) {
		DLMsg(errMsg, 1);
		OKfree(errMsg);
	} else
		DLMsg("COPulseTrainSourceVChan_Connected Error: Out of memory", 1);
	
}

/// HIFN Updates number of samples acquired when task controller is not active for analog and digital tasks.
static int ADNSamples_DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	// update only if task controller is not active
	if (taskActive) return 0;
	
	ADTaskSet_type*			tskSet			= GetVChanOwner(sinkVChan);
	int						error			= 0;
	char*					errMsg			= NULL;
	DataPacket_type*		dataPacket		= NULL;
	void*					dataPacketData	= NULL;
	DLDataTypes				dataPacketType;
	uInt64*					nSamplesPtr		= NULL;   
	
	if (tskSet->timing->measMode == MeasFinite) {
		errChk( GetDataPacket(tskSet->timing->nSamplesSinkVChan, &dataPacket, &errMsg) );
		dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType );
		switch (dataPacketType) {
			case DL_UChar:
				tskSet->timing->nSamples = (uInt64)**(unsigned char**)dataPacketData;
				break;
				
			case DL_UShort:
				tskSet->timing->nSamples = (uInt64)**(unsigned short**)dataPacketData;
				break;
				
			case DL_UInt:
				tskSet->timing->nSamples = (uInt64)**(unsigned int**)dataPacketData;
				break;
				
			case DL_ULongLong:
				tskSet->timing->nSamples = (uInt64)**(unsigned long long**)dataPacketData;
				break;
		}
			
		// update number of samples in dev structure
		DAQmxErrChk (DAQmxSetTimingAttribute(tskSet->taskHndl, DAQmx_SampQuant_SampPerChan, tskSet->timing->nSamples));
		// update number of samples in UI
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_NSamples, tskSet->timing->nSamples);
		// update duration in UI
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate);
	
		// cleanup
		ReleaseDataPacket(&dataPacket);
		
		// send data
		nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
		*nSamplesPtr = tskSet->timing->nSamples;
		dataPacket = init_DataPacket_type(DL_ULongLong, &nSamplesPtr,  NULL,NULL);
		errChk(SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errMsg));
	
	} else
		// n Samples cannot be used for continuous tasks, empty Sink VChan if there are any elements
		ReleaseAllDataPackets(tskSet->timing->nSamplesSinkVChan, NULL);
	
	
	return 0;

DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	
	// fall through
Error:
	
	// cleanup
	ReleaseDataPacket(&dataPacket);
	OKfree(nSamplesPtr);
	
	*errorInfo = FormatMsg(error, "ADNSamples_DataReceivedTC", errMsg);
	OKfree(errMsg);
	
	return error;

}

/// HIFN Receives sampling rate info for analog and digital IO DAQmx tasks when task controller is not active.
static int ADSamplingRate_DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	// update only if task controller is not active
	if (taskActive) return 0;
	
	ADTaskSet_type*			tskSet				= GetVChanOwner(sinkVChan);
	int						error				= 0;
	char*					errMsg				= NULL;
	DataPacket_type*		dataPacket			= NULL;
	void*					dataPacketData		= NULL;
	DLDataTypes				dataPacketType;
	double*					samplingRatePtr		= NULL;
	
	// get data packet
	errChk( GetDataPacket(sinkVChan, &dataPacket, &errMsg) );
	dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType);
	
	switch (dataPacketType) {
		case DL_Float:
			tskSet->timing->sampleRate = (double)**(float**)dataPacketData;
			break;
				
		case DL_Double:
			tskSet->timing->sampleRate = **(double**)dataPacketData;
			break;
	}
	
	// update sampling rate in dev structure
	DAQmxErrChk(DAQmxSetTimingAttribute(tskSet->taskHndl, DAQmx_SampClk_Rate, tskSet->timing->sampleRate));
	// update sampling rate in UI
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_SamplingRate, tskSet->timing->sampleRate /1000);	// display in [kHz]
	// update duration in UI
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate);
	
	// cleanup
	ReleaseDataPacket(&dataPacket);
	
	// send data
	nullChk( samplingRatePtr = malloc(sizeof(double)) );
	*samplingRatePtr = tskSet->timing->sampleRate;
	dataPacket = init_DataPacket_type(DL_Double, &samplingRatePtr,  NULL,NULL);
	errChk(SendDataPacket(tskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errMsg));
	
	return 0;

DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	
	// fall through
Error:
	
	// cleanup
	ReleaseDataPacket(&dataPacket);
	OKfree(samplingRatePtr);
	
	*errorInfo = FormatMsg(error, "ADSamplingRate_DataReceivedTC", errMsg);
	OKfree(errMsg);
	
	return error;
}

static int AO_DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	Dev_type*						dev					= GetTaskControlModuleData(taskControl);
	ChanSet_type*					chan				= GetVChanOwner((VChan_type*)sinkVChan);
	int								error				= 0;
	DataPacket_type**				dataPackets			= NULL;
	double** 						doubleDataPtrPtr	= NULL;
	float**							floatDataPtrPtr		= NULL;
	size_t							nPackets			= 0;
	size_t							nElem				= 0;
	char*							errMsg				= NULL;
	TaskHandle						taskHndl			= 0;
	ChanSet_AIAO_Voltage_type*		aoVoltageChan		= NULL;
	ChanSet_AIAO_Current_type*		aoCurrentChan		= NULL;
	DLDataTypes						dataPacketType;
	void*							dataPacketDataPtr	= NULL;
	double							doubleData;
	
	// process data only if task controller is not active
	if (taskActive) return 0;
	
	// set AO task to Verified state
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Abort) );  
	
	// get all available data packets
	errChk( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errMsg) );
				
	// create DAQmx task and channel
	switch (chan->chanType) {
						
		case Chan_AO_Voltage:
			aoVoltageChan = (ChanSet_AIAO_Voltage_type*) chan;				
			// create task
			DAQmxErrChk(DAQmxCreateTask("On demand AO voltage task", &taskHndl));
			// create DAQmx channel  
			DAQmxErrChk(DAQmxCreateAOVoltageChan(taskHndl, chan->name, "", aoVoltageChan->Vmin , aoVoltageChan->Vmax, DAQmx_Val_Volts, ""));  
			break;
							
		case Chan_AO_Current:
			aoCurrentChan = (ChanSet_AIAO_Current_type*) chan;				
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
						
			for (size_t i = 0; i < nPackets; i++) {
				// skip NULL packets
				if (!dataPackets[i]) continue;
						
				dataPacketDataPtr = GetDataPacketPtrToData(dataPackets[i], &dataPacketType);
				switch (dataPacketType) {
					case DL_Waveform_Double:
						doubleDataPtrPtr = GetWaveformPtrToData(*(Waveform_type**)dataPacketDataPtr, &nElem);
						for (size_t j = 0; j < nElem; j++) {							  // update AO one sample at a time from the provided waveform
							DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &(*doubleDataPtrPtr)[j], NULL, NULL));
							if (*abortFlag) break;
						}
						break;
						
					case DL_Waveform_Float:
						floatDataPtrPtr = GetWaveformPtrToData(*(Waveform_type**)dataPacketDataPtr, &nElem);
						for (size_t j = 0; j < nElem; j++) {							  // update AO one sample at a time from the provided waveform
							doubleData = (double) (*floatDataPtrPtr)[j];
							DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &doubleData, NULL, NULL));
							if (*abortFlag) break;
						}
						break;
									
					case DL_Float:
						doubleData = (double)(**(float**)dataPacketDataPtr);
						DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &doubleData, NULL, NULL));
						break;
									
					case DL_Double:
						DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, *(float64**)dataPacketDataPtr, NULL, NULL));
						break;
						
					default:
						
						// ignore repeated waveform data
						DLMsg("Error AO_DataReceivedTC: repeated waveforms not supported.\n\n", 1);
						break;
				}
						
			}
			break;
	}
	
	// cleanup
	for (size_t i = 0; i < nPackets; i++)
		ReleaseDataPacket(&dataPackets[i]); 
	
	DAQmxClearTask(taskHndl);
				
	OKfree(dataPackets); 
	
	// set AO task to Commited state
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Commit) );  
				
	return 0;
	
DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	*errorInfo = FormatMsg(error, "AO_DataReceivedTC", errMsg);
	OKfree(errMsg);
	
	// fall through
	
Error:
	
	// cleanup
	for (size_t i = 0; i < nPackets; i++)
		ReleaseDataPacket(&dataPackets[i]); 
	
	DAQmxClearTask(taskHndl);
				
	OKfree(dataPackets);
	
	return error;
}

static int DO_DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	return 0;
}

static int CO_DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	Dev_type*				dev					= GetTaskControlModuleData(taskControl);
	int						error				= 0;
	char*					errMsg				= NULL;
	DataPacket_type**		dataPackets			= NULL;
	DataPacket_type*		dataPacket			= NULL;
	size_t					nPackets			= 0;
	ChanSet_CO_type*		chanSetCO			= GetVChanOwner((VChan_type*)sinkVChan);	
	DLDataTypes				dataPacketType;  
	PulseTrain_type*    	pulseTrain;
	
	// update only if task controller is not active
	if (taskActive) return 0;
	
	// get all available data packets and use only the last packet
	errChk ( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errMsg) );
			
	for (size_t i = 0; i < nPackets-1; i++) 
		ReleaseDataPacket(&dataPackets[i]);  
				
	pulseTrain = *(PulseTrain_type**) GetDataPacketPtrToData(dataPackets[nPackets-1], &dataPacketType);
	discard_PulseTrain_type(&chanSetCO->pulseTrain); 
	nullChk( chanSetCO->pulseTrain = CopyPulseTrain(pulseTrain) );
	ReleaseDataPacket(&dataPackets[nPackets-1]); 
	
	// get channel panel handle to adjust UI settings
	int						timingPanHndl;
		
	GetPanelHandleFromTabPage(chanSetCO->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl); 
		
	double 					initialDelay;
	double					frequency;
	double					dutyCycle;
	double					lowTime;
	double					highTime;
	uInt32					initialDelayTicks;
	uInt32					lowTicks;
	uInt32					highTicks;
	PulseTrainIdleStates	idleState;
	PulseTrainModes			pulseMode;
	uInt64					nPulses;
	int						ctrlIdx;
		
	// set idle state
	idleState = GetPulseTrainIdleState(chanSetCO->pulseTrain);
	DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_IdleState, idleState) );
	// generation mode
	pulseMode = GetPulseTrainMode(chanSetCO->pulseTrain);
	// timing
	nPulses = GetPulseTrainNPulses(chanSetCO->pulseTrain);
	DAQmxErrChk ( DAQmxCfgImplicitTiming(chanSetCO->baseClass.taskHndl, pulseMode, nPulses) );
		
	switch (dataPacketType) {
		case DL_PulseTrain_Freq:
			// set initial delay
			initialDelay = GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq_InitialDelay, initialDelay) );
			SetCtrlVal(timingPanHndl, COFreqPan_InitialDelay, initialDelay); 
			// frequency
			frequency = GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq, frequency) );
			SetCtrlVal(timingPanHndl, COFreqPan_Frequency, frequency); 
			// duty cycle
			dutyCycle = GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_DutyCyc, dutyCycle/100) ); // normalize from [%] to 0-1 range
			SetCtrlVal(timingPanHndl, COFreqPan_DutyCycle, dutyCycle); // display in [%] 
			// set UI idle state
			switch (idleState) {
				case PulseTrainIdle_Low:
					SetCtrlIndex(timingPanHndl, COFreqPan_IdleState, 0);
					break;
						
				case PulseTrainIdle_High:
					SetCtrlIndex(timingPanHndl, COFreqPan_IdleState, 1);
					break;
			}
			// n pulses
			SetCtrlVal(timingPanHndl, COFreqPan_NPulses, nPulses);
			// pulse mode (daqmx settings set before)
			GetIndexFromValue(timingPanHndl, COFreqPan_Mode, &ctrlIdx, pulseMode); 
			SetCtrlIndex(timingPanHndl, COFreqPan_Mode, ctrlIdx);
			break;
				
		case DL_PulseTrain_Time:
			// set initial delay
			initialDelay = GetPulseTrainTimeTimingInitialDelay((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Time_InitialDelay, initialDelay) );
			SetCtrlVal(timingPanHndl, COTimePan_InitialDelay, initialDelay); 
			// low time
			lowTime = GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTime, lowTime) );
			SetCtrlVal(timingPanHndl, COTimePan_LowTime, lowTime); 
			// high time
			highTime = GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTime, highTime) );
			SetCtrlVal(timingPanHndl, COTimePan_HighTime, highTime);
			// set UI idle state
			switch (idleState) {
				case PulseTrainIdle_Low:
					SetCtrlIndex(timingPanHndl, COTimePan_IdleState, 0);
					break;
					
				case PulseTrainIdle_High:
					SetCtrlIndex(timingPanHndl, COTimePan_IdleState, 1);
					break;
			} 
			// n pulses
			SetCtrlVal(timingPanHndl, COTimePan_NPulses, nPulses);
			// pulse mode (daqmx settings set before)
			GetIndexFromValue(timingPanHndl, COTimePan_Mode, &ctrlIdx, pulseMode); 
			SetCtrlIndex(timingPanHndl, COTimePan_Mode, ctrlIdx);
			break;
			
		case DL_PulseTrain_Ticks:
			// tick delay
			initialDelayTicks = GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Ticks_InitialDelay, initialDelayTicks) );
			SetCtrlVal(timingPanHndl, COTicksPan_InitialDelay, initialDelayTicks); 
			// low ticks
			lowTicks = GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTicks, lowTicks) );
			SetCtrlVal(timingPanHndl, COTicksPan_LowTicks, lowTicks); 
			// high ticks
			highTicks = GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->baseClass.taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTicks, highTicks) );
			SetCtrlVal(timingPanHndl, COTicksPan_HighTicks, highTicks);
			// set UI idle state
			switch (idleState) {
				case PulseTrainIdle_Low:
					SetCtrlIndex(timingPanHndl, COTicksPan_IdleState, 0);
					break;
						
				case PulseTrainIdle_High:
					SetCtrlIndex(timingPanHndl, COTicksPan_IdleState, 1);
					break;
			}
			// n pulses
			SetCtrlVal(timingPanHndl, COTicksPan_NPulses, nPulses);
			// pulse mode (daqmx settings set before)
			GetIndexFromValue(timingPanHndl, COTicksPan_Mode, &ctrlIdx, pulseMode); 
			SetCtrlIndex(timingPanHndl, COTicksPan_Mode, ctrlIdx);
			break;
	}
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Send task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	// copy pulse train info
	nullChk( pulseTrain = CopyPulseTrain(chanSetCO->pulseTrain) );
	
	// generate data packet
	switch (GetPulseTrainType(chanSetCO->pulseTrain)) {
		
		case PulseTrain_Freq:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, &pulseTrain, NULL,discard_PulseTrain_type) );  
			break;
			
		case PulseTrain_Time:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, &pulseTrain, NULL,discard_PulseTrain_type) ); 
			break;
			
		case PulseTrain_Ticks:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, &pulseTrain, NULL,discard_PulseTrain_type) ); 
			break;
		
	}
	
	errChk( SendDataPacket(chanSetCO->baseClass.srcVChan, &dataPacket, FALSE, &errMsg) );
						
	OKfree(dataPackets);				
	
	return 0;

DAQmxError:
	
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0);
	errMsg = malloc((buffsize+1)*sizeof(char));
	DAQmxGetExtendedErrorInfo(errMsg, buffsize+1);
	// fall through	

Error:
	
	// cleanup
	discard_DataPacket_type(&dataPacket);
	
	*errorInfo = FormatMsg(error, "CO_DataReceivedTC", errMsg);
	OKfree(errMsg);
	return error;
}

//---------------------------------------------------------------------------------------------------------------------------------------
// NIDAQmx Device Task Controller Callbacks
//---------------------------------------------------------------------------------------------------------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	Dev_type*			dev			= GetTaskControlModuleData(taskControl);
	int					error		= 0;
	
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
	
	// configure device again
	errChk( ConfigDAQmxDevice(dev, errorInfo) );
	
	return 0;
	
Error:
	
	return error;
}

static int UnconfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	//Dev_type*	dev	= GetTaskControlModuleData(taskControl); 
	
	return 0; 
}

static void	IterateTC (TaskControl_type* taskControl, BOOL const* abortIterationFlag)
{
	Dev_type*		dev				= GetTaskControlModuleData(taskControl);
	char*			errMsg			= NULL;
	int				error			= 0;
	int*			nActiveTasksPtr	= NULL;	// Keeps track of the number of DAQmx tasks that must still complete
											// before a task controller iteration is considered to be complete
	
	// update iteration display
	
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, GetCurrentIterationIndex(GetTaskControlCurrentIter(taskControl)));
	
	//----------------------------------------
	// Count active tasks 
	//----------------------------------------
	
	// get active tasks counter handle
	if ((error = CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr)) < 0) {
		errMsg = FormatMsg(error, "IterateTC", "Could not obtain TSV handle");
		goto Error;
	}
	
	*nActiveTasksPtr = 0;
	
	// AI
	if (dev->AITaskSet)
		if (dev->AITaskSet->taskHndl)
			(*nActiveTasksPtr)++;
	// AO
	if (dev->AOTaskSet)
		if (dev->AOTaskSet->taskHndl)
			(*nActiveTasksPtr)++;
	// DI
	if (dev->DITaskSet)
		if (dev->DITaskSet->taskHndl)
			(*nActiveTasksPtr)++;
	// DO
	if (dev->DOTaskSet)
		if (dev->DOTaskSet->taskHndl)
			(*nActiveTasksPtr)++;
	// CI
	if (dev->CITaskSet) {
		size_t 				nCI = ListNumItems(dev->CITaskSet->chanTaskSet);
		ChanSet_type** 		chanSetPtr;
		for (size_t i = 1; i <= nCI; i++) {
			chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if ((*chanSetPtr)->taskHndl)
				(*nActiveTasksPtr)++;
		}
	}
	// CO
	if (dev->COTaskSet) {
		size_t 				nCO = ListNumItems(dev->COTaskSet->chanTaskSet);
		ChanSet_type** 		chanSetPtr;
		for (size_t i = 1; i <= nCO; i++) {
			chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
			if ((*chanSetPtr)->taskHndl)
				(*nActiveTasksPtr)++;
		}
	}
	
	// release active tasks counter handle
	if ((error=CmtReleaseTSVPtr(dev->nActiveTasks)) < 0) {
		errMsg = FormatMsg(error, "IterateTC", "Could not release TSV handle"); 
		goto Error;
	}
	
	//----------------------------------------
	// Launch DAQmx tasks
	//----------------------------------------
	// AI
	errChk( StartAIDAQmxTask(dev, &errMsg) );
	// AO
	errChk( StartAODAQmxTask(dev, &errMsg) );
	// DI
	errChk( StartDIDAQmxTask(dev, &errMsg) );
	// DO
	errChk( StartDODAQmxTask(dev, &errMsg) );
	// CI
	errChk( StartCIDAQmxTasks(dev, &errMsg) );
	// CO
	errChk( StartCODAQmxTasks(dev, &errMsg) );
		
	return; // no error
	
Error:
	
	TaskControlIterationDone(taskControl, error, errMsg, FALSE);
	OKfree(errMsg);
}

static void AbortIterationTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	Dev_type*			dev			= GetTaskControlModuleData(taskControl);
	char*				errMsg		= NULL;
	int					error		= 0;
	
	errChk( StopDAQmxTasks(dev, &errMsg) );
	
	return;
Error:
	
	TaskControlIterationDone(taskControl, error, errMsg, FALSE); 
	OKfree(errMsg);
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	//Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int DoneTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations,GetCurrentIterationIndex(GetTaskControlCurrentIter(taskControl)));
	
	return 0;
}

static int StoppedTC (TaskControl_type* taskControl,  BOOL const* abortFlag, char** errorInfo)
{
	int			error 	= 0;
	char*		errMsg	= NULL;
	Dev_type*	dev		= GetTaskControlModuleData(taskControl);
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, GetCurrentIterationIndex(GetTaskControlCurrentIter(taskControl)));
	
	return 0;
	
Error:
	
	if (!errMsg)
		errMsg = StrDup("Out of memory");
	
	*errorInfo = FormatMsg(error, "StoppedTC", errMsg);   
	OKfree(errMsg);
	
	return error; 
}

static int TaskTreeStatus (TaskControl_type* taskControl, TaskTreeExecution_type status, char** errorInfo)
{
	Dev_type*			dev			= GetTaskControlModuleData(taskControl);
	ChanSet_type**		chanSetPtr;
	size_t				nChans;	
	int					panHndl;
	
	// device panel
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IO, ATTR_DIMMED, (int) status);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IOMode, ATTR_DIMMED, (int) status);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IOType, ATTR_DIMMED, (int) status);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_PhysChan, ATTR_DIMMED, (int) status);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Mode, ATTR_DIMMED, (int) status);
	if (GetTaskControlMode(dev->taskController) == TASK_FINITE)
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, (int) status);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Wait, ATTR_DIMMED, (int) status);
	// AI
	if (dev->AITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->AITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtr)->chanPanHndl, ATTR_DIMMED, (int) status); 
		}
		// settings panel
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_SettingsTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) status);
		// timing panel
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TimingTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) status);
		// trigger panel
		int 	trigPanHndl;
		int		nTabs;
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TriggerTabIdx, &trigPanHndl);
		GetNumTabPages(trigPanHndl, Trig_TrigSet, &nTabs);
		for (size_t i = 0; i < nTabs; i++) {
			GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, i, &panHndl);
			SetPanelAttribute(panHndl, ATTR_DIMMED, (int) status);
		}
	}
	// AO
	if (dev->AOTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->AOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtr)->chanPanHndl, ATTR_DIMMED, (int) status); 
		}
		// settings panel
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_SettingsTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) status);
		// timing panel
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TimingTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) status);
		// trigger panel
		int 	trigPanHndl;
		int		nTabs;
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TriggerTabIdx, &trigPanHndl);
		GetNumTabPages(trigPanHndl, Trig_TrigSet, &nTabs);
		for (size_t i = 0; i < nTabs; i++) {
			GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, i, &panHndl);
			SetPanelAttribute(panHndl, ATTR_DIMMED, (int) status);
		}
	}
	// DI
	if (dev->DITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->DITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->DITaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtr)->chanPanHndl, ATTR_DIMMED, (int) status); 
		}
		
		// dim/undim rest of panels
	}
	// DO
	if (dev->DOTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->DOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
			SetPanelAttribute((*chanSetPtr)->chanPanHndl, ATTR_DIMMED, (int) status); 
		}
		
		// dim/undim rest of panels
	}
	// CI
	if (dev->CITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->CITaskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			SetPanelAttribute((*chanSetPtr)->chanPanHndl, ATTR_DIMMED, (int) status); 
		}
		
		// dim/undim rest of panels
	}
	// CO
	if (dev->COTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->COTaskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
			SetPanelAttribute((*chanSetPtr)->chanPanHndl, ATTR_DIMMED, (int) status); 
		}
		
		// dim/undim rest of panels
	}
	
	return 0;
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, 0);
	
	return 0;
}

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char errorMsg[])
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
}

static int ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo)
{
	//Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	return 0;
}
 
