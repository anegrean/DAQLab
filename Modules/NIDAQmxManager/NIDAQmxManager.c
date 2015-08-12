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
#include "DAQLabUtility.h"
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
#include "toolbox.h"

//========================================================================================================================================================================================================
// Macros

// DAQmxErrChk
#define DAQmxErrChk(fCall) if (errorInfo.error = (fCall), errorInfo.line = __LINE__, errorInfo.error < 0) \
{goto DAQmxError;} else
	
// obtains DAQmx error description and jumps to Error
#define DAQmx_ERROR_INFO { \
	int buffsize = DAQmxGetExtendedErrorInfo(NULL, 0); \
	nullChk( errorInfo.errMsg = malloc((buffsize + 1) * sizeof(char)) ); \
	errChk( DAQmxGetExtendedErrorInfo(errorInfo.errMsg, buffsize + 1) ); \
	goto Error; \
}

// Cmt library error macro
#define CmtErrChk(fCall) if (errorInfo.error = (fCall), errorInfo.line = __LINE__, errorInfo.error < 0) \ 
{goto CmtError;} else

// obtains Cmt error description and jumps to Error
#define Cmt_ERR { \
	if (errorInfo.error < 0) { \
		char CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE] = ""; \
		errChk( CmtGetErrorMessage(errorInfo.error, CmtErrMsgBuffer) ); \
		nullChk( errorInfo.errMsg = StrDup(CmtErrMsgBuffer) ); \
	} \
	goto Error; \
}

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
#define DAQmxDefault_Task_MeasMode						Operation_Finite
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

#define VChan_Data_Receive_Timeout						1e4						// timeout in [ms] to receive data

//========================================================================================================================================================================================================
// Types
typedef struct NIDAQmxManager 	NIDAQmxManager_type; 

typedef int CVICALLBACK (*CVICtrlCBFptr_type) (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


// Ranges for DAQmx IO, stored as pairs of low value and high value, e.g. -10 V, 10 V or -1 mA, 1 mA
typedef struct {
	int 					Nrngs;   					// number of ranges.
	unsigned int			selectedRange;				// selected range.
	double* 				low;  						// low value. 
	double* 				high; 						// high value.
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
} SampClockEdgeTypes;										// Sampling clock active edge.

// Measurement mode
typedef enum{
	Operation_Finite			= DAQmx_Val_FiniteSamps,
	Operation_Continuous		= DAQmx_Val_ContSamps
} OperationModes; 

// timing structure for ADC/DAC & digital sampling tasks
typedef struct {
	OperationModes 				measMode;      				// Measurement mode: finite or continuous.
	double       				sampleRate;    				// Target sampling rate in [Hz].
	double						targetSampleRate;			// Used in AI. This is the maximum sampling rate in [Hz] up to which the oversampling will be adjusted. The actual sampling rate used by the DAQ 
															// card will be the oversampling factor times the final sampling rate.
	uInt64        				nSamples;	    			// Target number of samples to be acquired in case of a finite recording.
	uInt32						oversampling;				// Oversampling factor, used in AI. This determines the actual sampling rate and the actual number of samples to be acquired
	BOOL						oversamplingAuto;			// Used in AI, auto-adjusts the oversampling factor based on the target DAQ sampling rate and the final sampling rate of the AI data.
	SinkVChan_type*				nSamplesSinkVChan;			// Used for receiving number of samples to be generated/received with each iteration of the DAQmx task controller.
															// data packets of DL_UChar, DL_UShort, DL_UInt and DL_UInt64 types.
	SourceVChan_type*			nSamplesSourceVChan;		// Used for sending number of samples for finite tasks.
															// data packets of DL_UInt64 type.
	SinkVChan_type*				samplingRateSinkVChan;		// Used for receiving sampling rate info with each iteration of the DAQmx task controller.  
															// data packets of DL_Double, DL_Float types.
	SourceVChan_type*			samplingRateSourceVChan;	// Used for sending sampling rate info.
															// data packets of DL_Double type.
	uInt32        				blockSize;     				// Number of samples for reading after which callbacks are called.
	char*         				sampClkSource; 				// Sample clock source if NULL then OnboardClock is used, otherwise the given clock
	SampClockEdgeTypes 			sampClkEdge;   				// Sample clock active edge.
	char*         				refClkSource;  				// Reference clock source used to sync internal clock, if NULL internal clock has no reference clock. 
	double        				refClkFreq;    				// Reference clock frequency if such a clock is used.
	char*						startSignalRouting;			// Task start signal routing.
	
	//------------------------------------
	// UI
	//------------------------------------
	int							timingPanHndl;				// Panel handle containing timing controls.
	int							settingsPanHndl;
} ADTaskTiming_type;

// timing structure for counter tasks
typedef struct {
	PulseTrainModes				measMode;      				// Measurement mode: finite or continuous.     
	uInt64        				nSamples;	    			// Total number of samples or pulses to be acquired/generated in case of a finite recording if device task settings is used as reference.
	char*         				sampClkSource; 				// Sample clock source if NULL then OnboardClock is used, otherwise the given clock
	SampClockEdgeTypes 			sampClkEdge;   				// Sample clock active edge.
	double       				sampleRate;    				// Sampling rate in [Hz] if device task settings is used as reference. 
	char*         				refClkSource;  				// Reference clock source used to sync internal clock, if NULL internal clock has no reference clock. 
	double        				refClkFreq;    				// Reference clock frequency if such a clock is used.
} CounterTaskTiming_type;

	
typedef struct {
	Trig_type 					trigType;					// Trigger type.
	char*     					trigSource;   				// Trigger source.
	double*						samplingRate;				// Reference to task sampling rate in [Hz].
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
	int							trigPanHndl;				// Trigger panel handle.
	int							trigSlopeCtrlID;			// Trigger settings control copy IDs
	int							trigSourceCtrlID;			// Trigger settings control copy IDs
	int							preTrigDurationCtrlID;		// Trigger settings control copy IDs
	int							preTrigNSamplesCtrlID;		// Trigger settings control copy IDs
	int							levelCtrlID;				// Trigger settings control copy IDs
	int							windowTrigCondCtrlID;		// Trigger settings control copy IDs
	int							windowTopCtrlID;			// Trigger settings control copy IDs
	int							windowBottomCtrlID;			// Trigger settings control copy IDs
} TaskTrig_type;

// Analog Input & Output channel type settings
typedef enum {
	Convert_To_Double,
	Convert_To_Float,
	Convert_To_UInt,
	Convert_To_UShort,
	Convert_To_UChar
} DataTypeConversions;

typedef struct {
	DataTypeConversions			dataType;					// Data type to which a double signal is converted to.
	double						min;						// Minimum value of signal to be converted.
	double						max;						// Maximum value of signal to be converted.
	double						offset;						// Offset added to the signal before data type conversion.
	double						gain;						// Gain applied to the signal before data type conversion.
} DataTypeConversion_type;

typedef struct ChanSet	ChanSet_type;
struct ChanSet {
	//DATA
	char*						name;						// Full physical channel name used by DAQmx, e.g. Dev1/ai1 
	Channel_type				chanType;					// Channel type.
	int							chanPanHndl;				// Panel handle where channel properties are adjusted
	Dev_type*					device;						// reference to device owing the channel
	SourceVChan_type*			srcVChan;					// Source VChan assigned to input type physical channels.
	SinkVChan_type*				sinkVChan;					// Sink VChan assigned to output type physical channels.
	BOOL						onDemand;					// If TRUE, the channel is updated/read out using software-timing.
	DataTypeConversion_type		dataTypeConversion;			// For input signals. Holds information on transforming the double valued input signal to other data types.
	BOOL						integrateFlag;				// For input signals. If True, then samples are integrated, otherwise, the oversamples are discarded.
	// METHODS
	DiscardFptr_type			discardFptr;				// Function pointer to discard specific channel data that is derived from this base class.					
};

// Chan_AI_Voltage
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	AIChannel_type*				chanAttr;					// Channel attributes.
	double        				VMax;      					// Maximum input/output voltage in [V].
	double        				VMin;       				// Minimum input/output voltage in [V].
	Terminal_type 				terminal;   				// Terminal type. 	
} ChanSet_AI_Voltage_type;

// Chan_AO_Voltage
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	AOChannel_type*				chanAttr;					// Channel attributes.
	double        				VMax;      					// Maximum input/output voltage in [V].
	double        				VMin;       				// Minimum input/output voltage in [V].
	Terminal_type 				terminal;   				// Terminal type. 	
} ChanSet_AO_Voltage_type;

typedef enum {
	ShuntResistor_Internal 		= DAQmx_Val_Internal,		// Use the internal shunt resistor for current measurements.
	ShuntResistor_External 		= DAQmx_Val_External		// Use an external shunt resistor for current measurement.
} ShuntResistor_type;

// Chan_AI_Current
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	AIChannel_type*				chanAttr;					// Channel attributes.
	double        				IMax;      					// Maximum voltage [A]
	double        				IMin;       				// Minimum voltage [A]
	ShuntResistor_type			shuntResistorType;			// Shunt resistor type.
	double						shuntResistorValue;			// If shuntResistorType = ShuntResistor_External, this gives the value of the shunt resistor in [Ohms] 
	Terminal_type 				terminal;   				// Terminal type 	
} ChanSet_AI_Current_type;

// Chan_AO_Current
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	AOChannel_type* 			chanAttr;					// Channel attributes.
	double        				IMax;      					// Maximum voltage [A]
	double        				IMin;       				// Minimum voltage [A]
	ShuntResistor_type			shuntResistorType;			// Shunt resistor type.
	double						shuntResistorValue;			// If shuntResistorType = ShuntResistor_External, this gives the value of the shunt resistor in [Ohms] 
	Terminal_type 				terminal;   				// Terminal type 	
} ChanSet_AO_Current_type;


// Digital Line/Port Input & Digital Line/Port Output channel type settings
// Chan_DI_Line
// Chan_DI_Port
// Chan_DO_Line
// Chan_DO_Port
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	BOOL 						invert;    					// invert polarity (for ports the polarity is inverted for all lines
} ChanSet_DIDO_type;

typedef enum {
	Count_Up					= DAQmx_Val_CountUp, 		// Increment the count register on each edge.
	Count_Down					= DAQmx_Val_CountDown, 		// Decrement the count register on each edge.
	Count_ExternalControl		= DAQmx_Val_ExtControlled 	// The state of a digital line controls the count direction. Each counter has a default count direction terminal.
} CountDirections;

typedef enum {
	Edge_Rising					= DAQmx_Val_Rising,
	Edge_Falling				= DAQmx_Val_Falling
} EdgeTypes;

// Chan_CI base class
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	TaskHandle					taskHndl;					// Each counter requires a separate task.
	EdgeTypes					activeEdge; 				// Specifies on which edges of the input signal to increment or decrement the count.
	TaskTrig_type* 				startTrig;     				// Task start trigger type. If NULL then there is no start trigger.
	TaskTrig_type* 				referenceTrig;     			// Task reference trigger type. If NULL then there is no reference trigger.
	HWTrigMaster_type*			HWTrigMaster;				// If this task is a master HW trigger, otherwise NULL.
	HWTrigSlave_type*			HWTrigSlave;				// If this task is a slave HW trigger, otherwise NULL.
} ChanSet_CI_type;

// Counter Input for Counting Edges, channel type settings
// Chan_CI_EdgeCount
typedef struct {
	ChanSet_CI_type				baseClass;					// Common CI settings parent class.
	uInt32 						initialCount;				// The value from which to start counting.
	CountDirections				countDirection; 			// Specifies whether to increment or decrement the counter on each edge.
} ChanSet_CI_EdgeCount_type;

// Counter Input for measuring the frequency of a digital signal
typedef enum {
	Meas_LowFreq1Ctr			= DAQmx_Val_LowFreq1Ctr,	// Use one counter that uses a constant timebase to measure the input signal.
	Meas_HighFreq2Ctr			= DAQmx_Val_HighFreq2Ctr,	// Use two counters, one of which counts pulses of the signal to measure during the specified measurement time.
	Meas_LargeRng2Ctr			= DAQmx_Val_LargeRng2Ctr    // Use one counter to divide the frequency of the input signal to create a lower-frequency signal 
						  									// that the second counter can more easily measure.
} CIFrequencyMeasMethods;

// Chan_CI_Frequency
typedef struct {
	ChanSet_CI_type				baseClass;					// Channel settings common to all channel types.
	double        				freqMax;      				// Maximum frequency expected to be measured [Hz]
	double        				freqMin;       				// Minimum frequency expected to be measured [Hz]
	CIFrequencyMeasMethods		measMethod;					// The method used to calculate the period or frequency of the signal. 
	double						measTime;					// The length of time to measure the frequency or period of a digital signal, when measMethod is DAQmx_Val_HighFreq2Ctr.
															// Measurement accuracy increases with increased measurement time and with increased signal frequency.
															// Caution: If you measure a high-frequency signal for too long a time, the count register could roll over, resulting in an incorrect measurement. 
	uInt32						divisor;					// The value by which to divide the input signal, when measMethod is DAQmx_Val_LargeRng2Ctr. The larger this value, the more accurate the measurement, 
															// but too large a value can cause the count register to roll over, resulting in an incorrect measurement.
	char*						freqInputTerminal;			// If other then default terminal assigned to the counter, otherwise this is NULL.
	CounterTaskTiming_type		taskTiming;					// Task timing info.
} ChanSet_CI_Frequency_type;

// Counter Output channel type settings
// Chan_CO_Pulse_Frequency
// Chan_CO_Pulse_Time
// Chan_CO_Pulse_Ticks
typedef struct {
	ChanSet_type				baseClass;					// Channel settings common to all channel types.
	TaskHandle					taskHndl;					// Only for counter type channels. Each counter requires a separate task.
	TaskTrig_type* 				startTrig;     				// Task start trigger type. If NULL then there is no start trigger.
	TaskTrig_type* 				referenceTrig;     			// Task reference trigger type. If NULL then there is no reference trigger.
	HWTrigMaster_type*			HWTrigMaster;				// If this task is a master HW trigger, otherwise NULL.
	HWTrigSlave_type*			HWTrigSlave;				// If this task is a slave HW trigger, otherwise NULL.
	PulseTrain_type*			pulseTrain;					// Pulse train info.
	char*         				clockSource;  				// Clock source used for CO ticks output. If NULL, internal clock has no reference clock. note: Clock source for CO Frequency and Co Time is Implicit.
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
} DAQmxIOMode_type;											

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
	BOOL*						nullPacketReceived;			// Array of BOOL equal to the number of DO channels. If TRUE, then the DO channel received a NULL packet and it will keep on generating
															// the last value until all DO channels have also received a NULL packet.
	int							writeBlocksLeftToWrite;		// Number of writeblocks left to write before the DO task stops. This guarantees that the last value of a given waveform is generated before the stop.
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
	ADTaskTiming_type*			timing;						// Task timing info
	HWTrigMaster_type*			HWTrigMaster;				// For establishing a task start HW-trigger dependency, this being a master.
	HWTrigSlave_type*			HWTrigSlave;				// For establishing a task start HW-trigger dependency, this being a slave.
	WriteAOData_type*			writeAOData;				// Used for continuous AO streaming. 
	ReadAIData_type*			readAIData;					// Used for processing of incoming AI data. If this is not an AI task, this is NULL.
	WriteDOData_type*			writeDOData;				// Used for continuous DO streaming.
	size_t						nOpenChannels;				// Counts the number of open VChans used for data exchange and is updated whenever a VChan is opened/closed.
} ADTaskSet_type;

// CI
typedef struct {
	// DAQmx task handles are specified per counter channel within chanTaskSet
	Dev_type*					dev;						// Reference to DAQmx device owing this data structure.			
	int							panHndl;					// Panel handle to task settings.
	TaskHandle					taskHndl;					// DAQmx task handle for hw-timed CI
	ListType 					chanTaskSet;   				// Channel and task settings. Of ChanSet_type*
	double        				timeout;       				// Task timeout [s]
} CITaskSet_type;

// CO
typedef struct {
	// DAQmx task handles are specified per counter channel chanTaskSet
	Dev_type*					dev;						// Reference to DAQmx device owing this data structure.			
	int							panHndl;					// Panel handle to task settings.
	TaskHandle					taskHndl;					// DAQmx task handle for hw-timed CO.
	ListType 					chanTaskSet;     			// Channel and task settings. Of ChanSet_CO_type*
	//double        				timeout;       				// Task timeout [s]
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
	int*						mainPanTopPos;
	int*						mainPanLeftPos;
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

static int							Load 									(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

	//---------------------------------------------------
	// Loading DAQmx settings
	//---------------------------------------------------

static int 							LoadCfg							 		(DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo); 

static int							LoadDeviceCfg							(NIDAQmxManager_type* NIDAQ, ActiveXMLObj_IXMLDOMElement_ NIDAQDeviceXMLElement, ERRORINFO* xmlErrorInfo);

static int							LoadADTaskCfg							(ADTaskSet_type* taskSet, ActiveXMLObj_IXMLDOMElement_ taskSetXMLElement, ERRORINFO* xmlErrorInfo);

static int							LoadCITaskCfg							(CITaskSet_type* taskSet, ActiveXMLObj_IXMLDOMElement_ taskSetXMLElement, ERRORINFO* xmlErrorInfo);

static int							LoadCOTaskCfg							(COTaskSet_type* taskSet, ActiveXMLObj_IXMLDOMElement_ taskSetXMLElement, ERRORINFO* xmlErrorInfo);

static int							LoadTaskTrigCfg							(TaskTrig_type* taskTrig, ActiveXMLObj_IXMLDOMElement_ triggerXMLElement, ERRORINFO* xmlErrorInfo);

static int							LoadChannelCfg							(Dev_type* dev, ChanSet_type** chanSetPtr, ActiveXMLObj_IXMLDOMElement_ channelXMLElement, ERRORINFO* xmlErrorInfo);


	//---------------------------------------------------
	// Saving DAQmx settings
	//---------------------------------------------------

static int							SaveCfg									(DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo);

static int 							SaveDeviceCfg 							(Dev_type* dev, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ NIDAQDeviceXMLElement, ERRORINFO* xmlErrorInfo); 

static int 							SaveADTaskCfg 							(ADTaskSet_type* taskSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement, ERRORINFO* xmlErrorInfo); 

static int 							SaveCITaskCfg 							(CITaskSet_type* taskSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement, ERRORINFO* xmlErrorInfo); 

static int 							SaveCOTaskCfg 							(COTaskSet_type* taskSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement, ERRORINFO* xmlErrorInfo); 

static int 							SaveTaskTrigCfg 						(TaskTrig_type* taskTrig, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ TriggerXMLElement, ERRORINFO* xmlErrorInfo);

static int 							SaveChannelCfg 							(ChanSet_type* chanSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ ChannelXMLElement, ERRORINFO* xmlErrorInfo);

static int 							DisplayPanels							(DAQLabModule_type* mod, BOOL visibleFlag); 

//---------------------------------------------
// Device and Channel data structure management
//---------------------------------------------

	// device data structure
static Dev_type* 					init_Dev_type 							(NIDAQmxManager_type* niDAQModule, DevAttr_type** attrPtr, char taskControllerName[]);
static void 						discard_Dev_type						(Dev_type** devPtr);

	// device detection
static ListType 					ListAllDAQmxDevices 					(void); 
static void 						UpdateDeviceList 						(ListType devList, int panHndl, int tableCtrl);
static void 						DiscardDeviceList 						(ListType* devList); 

	// device attributes data structure
static DevAttr_type* 				init_DevAttr_type						(void); 
static void 						discard_DevAttr_type					(DevAttr_type** attrPtr);
static DevAttr_type* 				copy_DevAttr_type						(DevAttr_type* attr);

	// AI channel data structure
static AIChannel_type* 				init_AIChannel_type						(void); 
static void 						discard_AIChannel_type					(AIChannel_type** chanPtr);

int CVICALLBACK 					DisposeAIChannelList	 				(size_t index, void* ptrToItem, void* callbackData);
static AIChannel_type* 				copy_AIChannel_type						(AIChannel_type* channel);
static ListType						copy_AIChannelList						(ListType chanList);
static AIChannel_type*				GetAIChannel							(Dev_type* dev, char physChanName[]);

	// AO channel data structure
static AOChannel_type* 				init_AOChannel_type						(void); 
static void 						discard_AOChannel_type					(AOChannel_type** chanPtr);

int CVICALLBACK 					DisposeAOChannelList 					(size_t index, void* ptrToItem, void*callbackData);
static AOChannel_type* 				copy_AOChannel_type						(AOChannel_type* channel);
static ListType						copy_AOChannelList						(ListType chanList);
static AOChannel_type*				GetAOChannel							(Dev_type* dev, char physChanName[]);
	
	// DI Line channel
static DILineChannel_type* 			init_DILineChannel_type					(void); 
static void 						discard_DILineChannel_type				(DILineChannel_type** chanPtr);

int CVICALLBACK 					DisposeDILineChannelList	 			(size_t index, void* ptrToItem, void* callbackData);
static DILineChannel_type* 			copy_DILineChannel_type					(DILineChannel_type* channel);
static ListType						copy_DILineChannelList					(ListType chanList);
static DILineChannel_type*			GetDILineChannel						(Dev_type* dev, char physChanName[]);

	// DI Port channel
static DIPortChannel_type* 			init_DIPortChannel_type					(void); 
static void 						discard_DIPortChannel_type				(DIPortChannel_type** chanPtr);

int CVICALLBACK 					DisposeDIPortChannelList 				(size_t index, void* ptrToItem, void* callbackData);
static DIPortChannel_type* 			copy_DIPortChannel_type					(DIPortChannel_type* channel);
static ListType						copy_DIPortChannelList					(ListType chanList);
static DIPortChannel_type*			GetDIPortChannel						(Dev_type* dev, char physChanName[]);

	// DO Line channel
static DOLineChannel_type* 			init_DOLineChannel_type					(void); 
static void 						discard_DOLineChannel_type				(DOLineChannel_type** chanPtr);

int CVICALLBACK 					DisposeDOLineChannelList	 			(size_t index, void* ptrToItem, void* callbackData);
static DOLineChannel_type* 			copy_DOLineChannel_type					(DOLineChannel_type* channel);
static ListType						copy_DOLineChannelList					(ListType chanList);
static DOLineChannel_type*			GetDOLineChannel						(Dev_type* dev, char physChanName[]);

	// DO Port channel
static DOPortChannel_type* 			init_DOPortChannel_type					(void); 
static void 						discard_DOPortChannel_type				(DOPortChannel_type** chanPtr);

int CVICALLBACK 					DisposeDOPortChannelList 				(size_t index, void* ptrToItem, void* callbackData);
static DOPortChannel_type* 			copy_DOPortChannel_type					(DOPortChannel_type* channel);
static ListType						copy_DOPortChannelList					(ListType chanList);
static DOPortChannel_type*			GetDOPortChannel						(Dev_type* dev, char physChanName[]);

	// CI channel
static CIChannel_type* 				init_CIChannel_type						(void); 
static void 						discard_CIChannel_type					(CIChannel_type** chanPtr);

int CVICALLBACK 					DisposeCIChannelList 					(size_t index, void* ptrToItem, void* callbackData);
static CIChannel_type* 				copy_CIChannel_type						(CIChannel_type* channel);
static ListType						copy_CIChannelList						(ListType chanList);
static CIChannel_type*				GetCIChannel							(Dev_type* dev, char physChanName[]);

	// CO channel
static COChannel_type* 				init_COChannel_type						(void); 
static void 						discard_COChannel_type					(COChannel_type** chanPtr);

int CVICALLBACK 					DisposeCOChannelList 					(size_t index, void* ptrToItem, void* callbackData);
static COChannel_type* 				copy_COChannel_type						(COChannel_type* channel);
static ListType						copy_COChannelList						(ListType chanList);
static COChannel_type*				GetCOChannel							(Dev_type* dev, char physChanName[]);

//------------------------------------------------------------------------------
// Channel settings
//------------------------------------------------------------------------------
	// channel settings base class
static void							init_ChanSet_type						(Dev_type* dev, ChanSet_type* chanSet, char physChanName[], Channel_type chanType, DiscardFptr_type discardFptr);
static void							discard_ChanSet_type					(ChanSet_type** chanSetPtr);

	//-----------------------------
	// AI
	//-----------------------------

	// AI Voltage
static ChanSet_AI_Voltage_type*		init_ChanSet_AI_Voltage_type 			(Dev_type* dev, char physChanName[], AIChannel_type* chanAttr, BOOL integrateFlag, 
													 						 DataTypeConversion_type dataTypeConversion, uInt32 VRangeIdx, Terminal_type terminalType);

static void							discard_ChanSet_AI_Voltage_type			(ChanSet_AI_Voltage_type** chanSetPtr);

static int 							AddToUI_Chan_AI_Voltage 				(ChanSet_AI_Voltage_type* chanSet);

static int CVICALLBACK				ChanSetAIVoltageUI_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// AI Current
static ChanSet_AI_Current_type*		init_ChanSet_AI_Current_type 			(Dev_type* dev, char physChanName[], AIChannel_type* chanAttr, BOOL integrateFlag, 
													 						 DataTypeConversion_type dataTypeConversion, uInt32 IRangeIdx, ShuntResistor_type shuntResistorType, 
																			 double shuntResistorValue, Terminal_type terminalType);

static void							discard_ChanSet_AI_Current_type			(ChanSet_AI_Current_type** chanSetPtr);                          

	// adjusts AI channel data type conversion gain and offset
void 								AdjustAIDataTypeGainOffset				(ChanSet_AI_Voltage_type* chSet, uInt32 oversampling);
void 								ResetAIDataTypeGainOffset				(ChanSet_AI_Voltage_type* chSet);


	//-----------------------------
	// AO
	//-----------------------------

	// AO Voltage
static ChanSet_AO_Voltage_type*		init_ChanSet_AO_Voltage_type 			(Dev_type* dev, char physChanName[], AOChannel_type* chanAttr, uInt32 VRangeIdx, Terminal_type terminalType);

static void							discard_ChanSet_AO_Voltage_type			(ChanSet_AO_Voltage_type** chanSetPtr);

static int 							AddToUI_Chan_AO_Voltage 				(ChanSet_AO_Voltage_type* chanSet); 

static int CVICALLBACK				ChanSetAOVoltageUI_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


	// AO Current
static ChanSet_AO_Current_type*		init_ChanSet_AO_Current_type 			(Dev_type* dev, char physChanName[], AOChannel_type* chanAttr, uInt32 IRangeIdx, ShuntResistor_type shuntResistorType, 
																			 double shuntResistorValue, Terminal_type terminalType);
static void							discard_ChanSet_AO_Current_type			(ChanSet_AO_Current_type** chanSetPtr);                          

	//-----------------------------
	// DI and DO 
	//-----------------------------
static ChanSet_DIDO_type*			init_ChanSet_DIDO_type					(Dev_type* dev, char physChanName[], Channel_type chanType, BOOL invert);
static void							discard_ChanSet_DIDO_type				(ChanSet_type** chanSetPtr);
static int 							ChanSetLineDO_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							ChanSetPortDO_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2); 
static int 							ReadDirectDigitalOut					(char* chan, uInt32* data, char** errorMsg);
static int 							DirectDigitalOut 						(char* chan, uInt32 data, BOOL invert, double timeout, char** errorMsg);
void 								SetPortControls							(int panel, uInt32 data);																	// <---------- check & cleanup!!!

	// CI base class
static void							init_ChanSet_CI_type					(Dev_type* dev, ChanSet_CI_type* chanSet, char physChanName[], Channel_type chanType, EdgeTypes activeEdge, DiscardFptr_type discardFptr);
static void							discard_ChanSet_CI_type					(ChanSet_CI_type** chanSetPtr);
	// CI Edge Count
static ChanSet_CI_EdgeCount_type* 	init_ChanSet_CI_EdgeCount_type 			(Dev_type* dev, char physChanName[], EdgeTypes activeEdge, uInt32 initialCount, CountDirections countDirection);
static void							discard_ChanSet_CI_EdgeCount_type		(ChanSet_CI_EdgeCount_type** chanSetPtr);
static int 							ChanSetCIEdge_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);   	// <---------- check & cleanup!!!

	// CI Frequency
static ChanSet_CI_Frequency_type* 	init_ChanSet_CI_Frequency_type 			(Dev_type* dev, char physChanName[], double freqMax, double freqMin, EdgeTypes activeEdge, CIFrequencyMeasMethods measMethod, 
																  			 double measTime, uInt32 divisor, char freqInputTerminal[], CounterTaskTiming_type taskTiming);
static void 						discard_ChanSet_CI_Frequency_type 		(ChanSet_CI_Frequency_type** chanSetPtr);

	// CO
static ChanSet_CO_type* 			init_ChanSet_CO_type 					(Dev_type* dev, char physChanName[], Channel_type chanType, char clockSource[], PulseTrain_type* pulseTrain);
static void 						discard_ChanSet_CO_type 				(ChanSet_CO_type** chanSetPtr);
static int 							ChanSetCO_CB 							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);	// <---------- check & cleanup!!!

	// channels & property lists
static ListType						GetPhysChanPropertyList					(char chanName[], int chanProperty);

//-------------------
// Channel management
//-------------------
static void							PopulateChannels						(Dev_type* dev);
static int							AddDAQmxChannel							(Dev_type* dev, DAQmxIO_type ioVal, DAQmxIOMode_type ioMode, int ioType, char chName[], char** errorMsg);
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
static ADTaskTiming_type*			init_ADTaskTiming_type					(void);
static void							discard_ADTaskTiming_type				(ADTaskTiming_type** taskTimingPtr);

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
static void							discardUI_ADTaskSet 					(ADTaskSet_type** taskSetPtr); 

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
static CITaskSet_type*				init_CITaskSet_type						(Dev_type* dev);
static void							discard_CITaskSet_type					(CITaskSet_type** taskSetPtr);

	// CO task settings
static COTaskSet_type*				init_COTaskSet_type						(Dev_type* dev);
static void							discard_COTaskSet_type					(COTaskSet_type** taskSetPtr);
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
static int							ConfigDAQmxDevice						(Dev_type* dev, char** errorMsg); 	 // configures all tasks on the device
static int							ConfigDAQmxAITask 						(Dev_type* dev, char** errorMsg);
static int							ConfigDAQmxAOTask 						(Dev_type* dev, char** errorMsg);
static int							ConfigDAQmxDITask 						(Dev_type* dev, char** errorMsg);
static int							ConfigDAQmxDOTask 						(Dev_type* dev, char** errorMsg);
static int							ConfigDAQmxCITask 						(Dev_type* dev, char** errorMsg);
static int							ConfigDAQmxCOTask 						(Dev_type* dev, char** errorMsg);
	// clears all CI DAQmx tasks
static int 							ClearDAQmxCITasks 						(Dev_type* dev, char** errorMsg);
	// clears all DAQmx Tasks defined for the device
static int 							ClearDAQmxTasks 						(Dev_type* dev, char** errorMsg); 
	// stops all DAQmx Tasks defined for the device
static int							StopDAQmxTasks 							(Dev_type* dev, char** errorMsg); 
	// starts DAQmx Tasks defined for the device
static int CVICALLBACK 				StartAIDAQmxTask_CB 					(void *functionData);
static int CVICALLBACK 				StartAODAQmxTask_CB 					(void *functionData);
static int CVICALLBACK 				StartDIDAQmxTask_CB 					(void *functionData);
static int CVICALLBACK 				StartDODAQmxTask_CB 					(void *functionData);
static int CVICALLBACK 				StartCIDAQmxTasks_CB 					(void *functionData);
static int CVICALLBACK 				StartCODAQmxTasks_CB 					(void *functionData);

//---------------------
// DAQmx task callbacks
//---------------------
	// AI
static int 							SendAIBufferData 						(Dev_type* dev, ChanSet_type* AIChSet, size_t chIdx, int nRead, float64* AIReadBuffer, char** errorMsg); 
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
static int				 			WriteAODAQmx 							(Dev_type* dev, char** errorMsg); 

	// DO
//FCallReturn_type*   				WriteDODAQmx							(Dev_type* dev); 					// <------------------ cleanup!!!

//--------------------------------------------
// Various DAQmx module managemement functions
//--------------------------------------------

static char* 						substr									(const char* token, char** idxstart);

static BOOL							ValidTaskControllerName					(char name[], void* dataPtr);

//--------------------------------------------
// DAQmx conversion functions
//--------------------------------------------

static int32						PulseTrainIdleStates_To_DAQmxVal		(PulseTrainIdleStates idleVal);

static int32						PulseTrainModes_To_DAQmxVal				(PulseTrainModes pulseMode);

//---------------------------
// DAQmx device management
//---------------------------

static int CVICALLBACK 				MainPan_CB								(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			ManageDevPan_CB 						(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static int 							AddDAQmxDeviceToManager 				(Dev_type* dev); 

int CVICALLBACK 					ManageDevices_CB 						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static void CVICALLBACK 			DeleteDev_CB 							(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);

static void 						UnregisterDeviceFromFramework 			(Dev_type* dev);

//---------------------------
// Trigger settings callbacks
//---------------------------

static int CVICALLBACK 				TaskStartTrigType_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static void							AddStartTriggerToUI						(TaskTrig_type* taskTrig);
static int CVICALLBACK 				TaskReferenceTrigType_CB				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static void 						AddReferenceTriggerToUI 				(TaskTrig_type* taskTrig);
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
	// VChan open/close callbacks
	//-----------------------------------

	// AI
static void							AIDataVChan_StateChange					(VChan_type* self, void* VChanOwner, VChanStates state);
	// AO
static void							AODataVChan_StateChange					(VChan_type* self, void* VChanOwner, VChanStates state);

	//----------------------
	// AI, AO, DI & DO Tasks
	//----------------------

	// receiving number of samples
static void							ADNSamplesSinkVChan_StateChange			(VChan_type* self, void* VChanOwner, VChanStates state);
	// sending number of samples
static void							ADNSamplesSourceVChan_StateChange		(VChan_type* self, void* VChanOwner, VChanStates state);
	// receiving sampling rate
static void							ADSamplingRateSinkVChan_StateChange		(VChan_type* self, void* VChanOwner, VChanStates state);
	// sending sampling rate
static void							ADSamplingRateSourceVChan_StateChange	(VChan_type* self, void* VChanOwner, VChanStates state);

	//----------------
	// CO Task
	//----------------

	// receiving CO pulse train info
static void							COPulseTrainSinkVChan_StateChange		(VChan_type* self, void* VChanOwner, VChanStates state);
	// sending CO pulse train info
static void							COPulseTrainSourceVChan_StateChange		(VChan_type* self, void* VChanOwner, VChanStates state);

	//------------------------------------------------------------------- 
	// VChan receiving data callbacks when task controller is not active
	//-------------------------------------------------------------------


	// AI, AO, Di & DO 
static int							ADNSamples_DataReceivedTC				(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);
static int							ADSamplingRate_DataReceivedTC			(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);
	// AO
static int							AO_DataReceivedTC						(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);
	// DO
static int							DO_DataReceivedTC						(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);
	// CO
static int				 			CO_DataReceivedTC						(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);



//---------------------------------------------------- 
// NIDAQmx Device Task Controller Callbacks
//---------------------------------------------------- 

static int							ConfigureTC								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int							UnconfigureTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static void							IterateTC								(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag);

static int							StartTC									(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int							DoneTC									(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int							StoppedTC								(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int 							IterationStopTC 						(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int							TaskTreeStateChange 					(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);

static int				 			ResetTC 								(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 

static void				 			ErrorTC 								(TaskControl_type* taskControl, int errorID, char errorMsg[]);

static int							ModuleEventHandler						(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg);  

//========================================================================================================================================================================================================
// Global variables

ListType	allInstalledDevices		= 0;		// List of DAQ devices available of DevAttr_type*.


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
	nidaq->mainPanTopPos			= NULL;
	nidaq->mainPanLeftPos			= NULL;
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
	
	//discard global devList
	DiscardDeviceList(&allInstalledDevices);
	
	// discard UI
	OKfree(nidaq->mainPanTopPos);
	OKfree(nidaq->mainPanLeftPos);
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

int	Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
#define Load_Err_ChannelNotImplemented	-1
	
INIT_ERR	
	
	NIDAQmxManager_type* 	nidaq						= (NIDAQmxManager_type*) mod;  
	int						menuItemDevicesHndl			= 0;
	size_t					nDevs						= ListNumItems(nidaq->DAQmxDevices);
	Dev_type*				DAQmxDev					= NULL;
	
	// main panel and apply stored position if any 
	errChk(nidaq->mainPanHndl = LoadPanel(workspacePanHndl, MOD_NIDAQmxManager_UI, NIDAQmxPan));
	
	if(nidaq->mainPanLeftPos)
		SetPanelAttribute(nidaq->mainPanHndl, ATTR_LEFT, *nidaq->mainPanLeftPos);
	if(nidaq->mainPanTopPos)
		SetPanelAttribute(nidaq->mainPanHndl, ATTR_TOP, *nidaq->mainPanTopPos); 
	
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
	// dim "Delete" menu item if there are no devices to be deleted
	if (nDevs) 
		SetMenuBarAttribute(nidaq->menuBarHndl, nidaq->deleteDevMenuItemID, ATTR_DIMMED, FALSE);
	else
		SetMenuBarAttribute(nidaq->menuBarHndl, nidaq->deleteDevMenuItemID, ATTR_DIMMED, TRUE);
	
	// populate devices and DAQmx tasks
	ChanSet_type*   chanSet = NULL;
	size_t			nChans	= 0;
	for (size_t i = 1; i <= nDevs; i++) {
		DAQmxDev = *(Dev_type**)ListGetPtrToItem(nidaq->DAQmxDevices, i);
		errChk( AddDAQmxDeviceToManager(DAQmxDev) );
		
		//-------------------------------
		// AI task
		//-------------------------------
		if (DAQmxDev->AITaskSet) {
			
			newUI_ADTaskSet(DAQmxDev->AITaskSet, DAQmxAITaskSetTabName, RemoveDAQmxAIChannel_CB, DAQmxDev->attr->AITriggerUsage, SinkVChan_AINSamples_BaseName, 
							SourceVChan_AINSamples_BaseName, SinkVChan_AISamplingRate_BaseName, SourceVChan_AISamplingRate_BaseName, HWTrig_AI_BaseName);
		
			// AI channels
			nChans = ListNumItems(DAQmxDev->AITaskSet->chanSet);
			for (size_t j = 1; j <= nChans; j++) {
				chanSet = *(ChanSet_type**) ListGetPtrToItem(DAQmxDev->AITaskSet->chanSet, j);
				switch(chanSet->chanType) {
						
					case Chan_AI_Voltage:
						
						AddToUI_Chan_AI_Voltage((ChanSet_AI_Voltage_type*)chanSet);
						break;
				
					// add here more channel types	
					
					default:
						SET_ERR(Load_Err_ChannelNotImplemented, "Channel not implemented");
				}
			}
		}
		
		//-------------------------------
		// AO task
		//-------------------------------
		if(DAQmxDev->AOTaskSet) {
			
			newUI_ADTaskSet(DAQmxDev->AOTaskSet, DAQmxAOTaskSetTabName, RemoveDAQmxAOChannel_CB, DAQmxDev->attr->AOTriggerUsage, SinkVChan_AONSamples_BaseName, 
							SourceVChan_AONSamples_BaseName, SinkVChan_AOSamplingRate_BaseName, SourceVChan_AOSamplingRate_BaseName, HWTrig_AO_BaseName); 
		
			// AO channels
			nChans = ListNumItems(DAQmxDev->AOTaskSet->chanSet);
			for (size_t j = 1; j <= nChans; j++) {
				chanSet = *(ChanSet_type**) ListGetPtrToItem(DAQmxDev->AOTaskSet->chanSet, j);
				switch(chanSet->chanType) {
						
					case Chan_AO_Voltage:
						
						AddToUI_Chan_AO_Voltage((ChanSet_AO_Voltage_type*)chanSet);
						break;
				
					// add here more channel types	
					
					default:
						SET_ERR(Load_Err_ChannelNotImplemented, "Channel not implemented");
				}
			}
		
		}
		
		//-------------------------------
		// DI task
		//-------------------------------
		
		//-------------------------------
		// DO task
		//-------------------------------
		
		//-------------------------------
		// CI task
		//-------------------------------
		
		//-------------------------------
		// CO task
		//-------------------------------
		
		// configure Task Controller
		errChk( TaskControlEvent(DAQmxDev->taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
	
	}
	
	
	return 0;
	
Error:
	
	// cleanup
	OKfreePanHndl(nidaq->mainPanHndl);
	OKfreePanHndl(nidaq->devListPanHndl);
	OKfreePanHndl(nidaq->taskSetPanHndl); 
	
RETURN_ERR
}

static int LoadCfg (DAQLabModule_type* mod, ActiveXMLObj_IXMLDOMElement_ moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	
	NIDAQmxManager_type*	NIDAQ				= (NIDAQmxManager_type*) mod;
	size_t					nInstalledDevs		= 0;
	
	//--------------------------------------------------------------------------
	// Find devices and load their attributes
	//--------------------------------------------------------------------------
	
	DiscardDeviceList(&allInstalledDevices);
	nullChk( allInstalledDevices = ListAllDAQmxDevices() );
	nInstalledDevs = ListNumItems(allInstalledDevices);
	//--------------------------------------------------------------------------
	// Load main panel position
	//--------------------------------------------------------------------------
	
	NIDAQ->mainPanTopPos								= malloc(sizeof(int));
	NIDAQ->mainPanLeftPos								= malloc(sizeof(int));
	
	DAQLabXMLNode 			NIDAQAttr[] 				= {	{"PanTopPos", BasicData_Int, NIDAQ->mainPanTopPos},
											  		   		{"PanLeftPos", BasicData_Int, NIDAQ->mainPanLeftPos} };
														
	errChk( DLGetXMLElementAttributes(moduleElement, NIDAQAttr, NumElem(NIDAQAttr)) ); 
	
	//--------------------------------------------------------------------------
	// Load DAQmx devices if they are still present
	//--------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMNodeList_	deviceNodeList		= 0;
	ActiveXMLObj_IXMLDOMNode_		deviceNode			= 0;      
	long							nXMLDevices			= 0;
	
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(moduleElement, xmlErrorInfo, "NIDAQDevice", &deviceNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(deviceNodeList, xmlErrorInfo, &nXMLDevices) );
	
	// load device settings if at least a device is installed
	if (nInstalledDevs)
		for (long i = 0; i < nXMLDevices; i++) {
			errChk( ActiveXML_IXMLDOMNodeList_Getitem(deviceNodeList, xmlErrorInfo, i, &deviceNode) );
			errChk( LoadDeviceCfg (NIDAQ, (ActiveXMLObj_IXMLDOMElement_) deviceNode, xmlErrorInfo) );
			OKfreeCAHndl(deviceNode);
		}
	
	OKfreeCAHndl(deviceNodeList);
	
	return 0;
	
Error:

	return errorInfo.error;
}

static int LoadDeviceCfg (NIDAQmxManager_type* NIDAQ, ActiveXMLObj_IXMLDOMElement_ NIDAQDeviceXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR	
	
	unsigned int					productNum			= 0;
	unsigned int					serial				= 0;
	char*							taskControllerName	= NULL;
	DevAttr_type*					installedDevAttr	= NULL;
	DevAttr_type*					deviceToLoadAttr	= NULL;
	DevAttr_type*					deviceAttrCopy		= NULL;
	Dev_type*						newDev				= NULL;
	ADTaskSet_type* 				ADTaskSet			= NULL;
	CITaskSet_type*					CITaskSet			= NULL;
	COTaskSet_type*					COTaskSet			= NULL;
	size_t							nDevs				= ListNumItems(allInstalledDevices);
	ActiveXMLObj_IXMLDOMElement_	taskSetXMLElement	= 0;
	DAQLabXMLNode 					deviceXMLAttr[]		= {	{"NIProductClassID", 	BasicData_UInt, 	&productNum},
															{"DeviceSerialNumber",	BasicData_UInt,		&serial},
															{"Name", 				BasicData_CString, 	&taskControllerName}	};
	
	//-------------------------------------------------------------------------------------------------
	// Try to load XML device
	//-------------------------------------------------------------------------------------------------
													
	errChk( DLGetXMLElementAttributes(NIDAQDeviceXMLElement, deviceXMLAttr, NumElem(deviceXMLAttr)) );
	
	// check first if device with the same serial number is present in the system
	for (size_t i = 1; i <= nDevs; i++) {
		installedDevAttr = *(DevAttr_type**) ListGetPtrToItem(allInstalledDevices, i);
		if (installedDevAttr->serial == serial) {
			deviceToLoadAttr = installedDevAttr;
			break;
		}
	}
	
	// if device with the same serial number was not found, try to load another device of the same class
	if (!deviceToLoadAttr)
		for (size_t i = 1; i <= nDevs; i++) {
			installedDevAttr = *(DevAttr_type**) ListGetPtrToItem(allInstalledDevices, i);
			if (installedDevAttr->serial == serial) {
				deviceToLoadAttr = installedDevAttr;
				break;
			}
		}
	
	// if still no device was found, then exit this function and try another device
	if (!deviceToLoadAttr) return 0;
	
	//-------------------------------------------------------------------------------------------------
	// Create new device with loaded XML settings
	//-------------------------------------------------------------------------------------------------
	
	nullChk( deviceAttrCopy = copy_DevAttr_type(deviceToLoadAttr) );
	nullChk( newDev = init_Dev_type(NIDAQ, &deviceAttrCopy, taskControllerName) );
	
	//-------------------------------------------------------------------------------------------------
	// Load device tasks
	//-------------------------------------------------------------------------------------------------
	
	// AI
	//-----------------------------------
	errChk( DLGetSingleXMLElementFromElement(NIDAQDeviceXMLElement, "AITask", &taskSetXMLElement) );
	if (taskSetXMLElement) {
		nullChk( ADTaskSet = init_ADTaskSet_type(newDev) );
		errChk(	LoadADTaskCfg (ADTaskSet, taskSetXMLElement, xmlErrorInfo) );
		newDev->AITaskSet = ADTaskSet;
		OKfreeCAHndl(taskSetXMLElement);
	}
	
	// AO
	//-----------------------------------
	errChk( DLGetSingleXMLElementFromElement(NIDAQDeviceXMLElement, "AOTask", &taskSetXMLElement) );
	if (taskSetXMLElement) {
		nullChk( ADTaskSet = init_ADTaskSet_type(newDev) );
		errChk(	LoadADTaskCfg (ADTaskSet, taskSetXMLElement, xmlErrorInfo) );
		newDev->AOTaskSet = ADTaskSet;
		OKfreeCAHndl(taskSetXMLElement);
	}
	
	// DI
	//-----------------------------------
	errChk( DLGetSingleXMLElementFromElement(NIDAQDeviceXMLElement, "DITask", &taskSetXMLElement) );
	if (taskSetXMLElement) {
		nullChk( ADTaskSet = init_ADTaskSet_type(newDev) );
		errChk(	LoadADTaskCfg (ADTaskSet, taskSetXMLElement, xmlErrorInfo) );
		newDev->DITaskSet = ADTaskSet;
		OKfreeCAHndl(taskSetXMLElement);
	}
	
	// DO
	//-----------------------------------
	errChk( DLGetSingleXMLElementFromElement(NIDAQDeviceXMLElement, "DOTask", &taskSetXMLElement) );
	if (taskSetXMLElement) {
		nullChk( ADTaskSet = init_ADTaskSet_type(newDev) );
		errChk(	LoadADTaskCfg (ADTaskSet, taskSetXMLElement, xmlErrorInfo) );
		newDev->DOTaskSet = ADTaskSet;
		OKfreeCAHndl(taskSetXMLElement);
	}
	
	// CI
	//-----------------------------------
	errChk( DLGetSingleXMLElementFromElement(NIDAQDeviceXMLElement, "CITask", &taskSetXMLElement) );
	if (taskSetXMLElement) {
		nullChk( CITaskSet = init_CITaskSet_type(newDev) );
		errChk(	LoadCITaskCfg (CITaskSet, taskSetXMLElement, xmlErrorInfo) );
		newDev->CITaskSet = CITaskSet;
		OKfreeCAHndl(taskSetXMLElement);
	}
	
	// CO
	//-----------------------------------
	errChk( DLGetSingleXMLElementFromElement(NIDAQDeviceXMLElement, "COTask", &taskSetXMLElement) );
	if (taskSetXMLElement) {
		nullChk( COTaskSet = init_COTaskSet_type(newDev) );
		errChk(	LoadCOTaskCfg (COTaskSet, taskSetXMLElement, xmlErrorInfo) );
		newDev->COTaskSet = COTaskSet;
		OKfreeCAHndl(taskSetXMLElement);
	}
	
	
	ListInsertItem(NIDAQ->DAQmxDevices, &newDev, END_OF_LIST);
	
	// cleanup
	OKfree(taskControllerName);

	return 0;
	
Error:
	OKfreeCAHndl(taskSetXMLElement);
	discard_ADTaskSet_type(&ADTaskSet);
	discard_CITaskSet_type(&CITaskSet);
	discard_COTaskSet_type(&COTaskSet);
	OKfree(taskControllerName);
	discard_DevAttr_type(&deviceAttrCopy);
	discard_Dev_type(&newDev);
	
	return errorInfo.error;
}

static int LoadADTaskCfg (ADTaskSet_type* taskSet, ActiveXMLObj_IXMLDOMElement_ taskSetXMLElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR

	Dev_type*						dev								= taskSet->dev;
	ChanSet_type*					chanSet							= NULL;
	ActiveXMLObj_IXMLDOMElement_	channelsXMLElement				= 0;
	ActiveXMLObj_IXMLDOMNodeList_	channelNodeList					= 0;
	ActiveXMLObj_IXMLDOMNode_		channelNode						= 0; 
	ActiveXMLObj_IXMLDOMElement_	taskTriggersXMLElement			= 0;
	ActiveXMLObj_IXMLDOMElement_	startTriggerXMLElement			= 0;
	ActiveXMLObj_IXMLDOMElement_	referenceTriggerXMLElement		= 0;
	long							nChans							= 0;
	uInt32							operationMode					= 0;			
	uInt32							sampleClockEdge					= 0;			
	DAQLabXMLNode 					taskAttr[] 						= {	{"Timeout", 					BasicData_Double, 		&taskSet->timeout},
																		{"OperationMode", 				BasicData_UInt, 		&operationMode},
																		{"SamplingRate", 				BasicData_Double, 		&taskSet->timing->sampleRate},
																		{"TargetSamplingRate",			BasicData_Double,		&taskSet->timing->targetSampleRate},
																		{"NSamples", 					BasicData_UInt64, 		&taskSet->timing->nSamples},
																		{"Oversampling",				BasicData_UInt,			&taskSet->timing->oversampling},
																		{"OversamplingAutoAdjust",		BasicData_Bool,			&taskSet->timing->oversamplingAuto}, 
																		{"BlockSize", 					BasicData_UInt, 		&taskSet->timing->blockSize},
																		{"SampleClockSource", 			BasicData_CString, 		&taskSet->timing->sampClkSource},
																		{"SampleClockEdge", 			BasicData_UInt, 		&sampleClockEdge},
																		{"ReferenceClockSource", 		BasicData_CString, 		&taskSet->timing->refClkSource},
																		{"ReferenceClockFrequency", 	BasicData_Double, 		&taskSet->timing->refClkFreq},
																		{"StartSignalRouting",			BasicData_CString,		&taskSet->timing->startSignalRouting}	};
	
	errChk( DLGetXMLElementAttributes(taskSetXMLElement, taskAttr, NumElem(taskAttr)) );
	// assign remaining attributes
	taskSet->timing->measMode = (OperationModes) operationMode;
	taskSet->timing->sampClkEdge = (SampClockEdgeTypes) sampleClockEdge;
	
	//--------------------------------------------------------------------------------
	// Load start and reference triggers
	//--------------------------------------------------------------------------------

	errChk( DLGetSingleXMLElementFromElement(taskSetXMLElement, "Triggers", &taskTriggersXMLElement) );
	if (taskTriggersXMLElement) {
		errChk( DLGetSingleXMLElementFromElement(taskTriggersXMLElement, "StartTrigger", &startTriggerXMLElement) );
		errChk( DLGetSingleXMLElementFromElement(taskTriggersXMLElement, "ReferenceTrigger", &referenceTriggerXMLElement) );
	}
	
	if (startTriggerXMLElement) {
		nullChk( taskSet->startTrig = init_TaskTrig_type(dev, &taskSet->timing->sampleRate) );
		errChk( LoadTaskTrigCfg(taskSet->startTrig, startTriggerXMLElement, xmlErrorInfo) );
	}
	
	if (referenceTriggerXMLElement) {
		nullChk( taskSet->referenceTrig = init_TaskTrig_type(dev, &taskSet->timing->sampleRate) );
		errChk( LoadTaskTrigCfg(taskSet->referenceTrig, referenceTriggerXMLElement, xmlErrorInfo) );
	}
	
	
	//--------------------------------------------------------------------------------
	// Load task channels
	//--------------------------------------------------------------------------------
	errChk( DLGetSingleXMLElementFromElement(taskSetXMLElement, "Channels", &channelsXMLElement) );
	
	errChk ( ActiveXML_IXMLDOMElement_getElementsByTagName(channelsXMLElement, xmlErrorInfo, "Channel", &channelNodeList) );
	errChk ( ActiveXML_IXMLDOMNodeList_Getlength(channelNodeList, xmlErrorInfo, &nChans) );
	
	for (long i = 0; i < nChans; i++) {
		errChk ( ActiveXML_IXMLDOMNodeList_Getitem(channelNodeList, xmlErrorInfo, i, &channelNode) );
		errChk( LoadChannelCfg(dev, &chanSet, (ActiveXMLObj_IXMLDOMElement_) channelNode, xmlErrorInfo) );
		OKfreeCAHndl(channelNode);
		ListInsertItem(taskSet->chanSet, &chanSet, END_OF_LIST);
	}
	
	//-------------------
	// cleanup
	//-------------------
	//free  string attributes
	OKfree(*(char**)taskAttr[8].pData);
	OKfree(*(char**)taskAttr[10].pData);
	
	OKfreeCAHndl(channelsXMLElement);
	OKfreeCAHndl(channelNodeList);
	OKfreeCAHndl(taskTriggersXMLElement);
	OKfreeCAHndl(startTriggerXMLElement);
	OKfreeCAHndl(referenceTriggerXMLElement);
	
	return 0;
	
Error:
	
	OKfreeCAHndl(channelsXMLElement);
	OKfreeCAHndl(channelNodeList);
	OKfreeCAHndl(channelNode);
	OKfreeCAHndl(taskTriggersXMLElement);
	OKfreeCAHndl(startTriggerXMLElement);
	OKfreeCAHndl(referenceTriggerXMLElement);
	
	return errorInfo.error;
}

static int LoadCITaskCfg (CITaskSet_type* taskSet, ActiveXMLObj_IXMLDOMElement_ taskSetXMLElement, ERRORINFO* xmlErrorInfo)
{
	return 0;
	
}

static int LoadCOTaskCfg (COTaskSet_type* taskSet, ActiveXMLObj_IXMLDOMElement_ taskSetXMLElement, ERRORINFO* xmlErrorInfo)
{
	return 0;
	
}

static int LoadTaskTrigCfg (TaskTrig_type* taskTrig, ActiveXMLObj_IXMLDOMElement_ triggerXMLElement, ERRORINFO* xmlErrorInfo)
{
#define LoadTaskTrigCfg_Err_NotImplemented		-1

INIT_ERR
	
	//ERRORINFO						xmlErrorInfo;
	
	uInt32							trigType				= 0;
	uInt32							slopeType				= 0;
	uInt32							triggerCondition		= 0;
	
		// shared trigger attributes
	DAQLabXMLNode 					sharedTrigAttr[]		= {	{"Type", 					BasicData_UInt, 	&trigType},
																{"Source", 					BasicData_CString, 	&taskTrig->trigSource} };
		// for both analog and digital edge triggers
	DAQLabXMLNode 					edgeTrigAttr[]			= {	{"Slope", 					BasicData_UInt, 	&slopeType} };
		// for analog edge trigger
	DAQLabXMLNode 					levelTrigAttr[]			= {	{"Level", 					BasicData_Double, 	&taskTrig->level} };
		// for analog window trigger
	DAQLabXMLNode 					windowTrigAttr[]		= {	{"WindowTop", 				BasicData_Double, 	&taskTrig->wndTop},
																{"WindowBottom", 			BasicData_Double, 	&taskTrig->wndBttm},
																{"WindowTriggerCondition", 	BasicData_UInt, 	&triggerCondition} };
																
	// load shared trigger attributes
	errChk( DLGetXMLElementAttributes(triggerXMLElement, sharedTrigAttr, NumElem(sharedTrigAttr)) );
	
	taskTrig->trigType 		= (Trig_type) trigType; 
	
	
	// add specific trigger attributes
	switch (taskTrig->trigType) {
			
		case Trig_None:
			
			break;
			
		case Trig_DigitalEdge:
			
			errChk( DLGetXMLElementAttributes(triggerXMLElement, edgeTrigAttr, NumElem(edgeTrigAttr)) );
			taskTrig->slope = (TrigSlope_type) slopeType;
			break;
			
		case Trig_DigitalPattern:
			
			return LoadTaskTrigCfg_Err_NotImplemented;
			
		case Trig_AnalogEdge:
			
			errChk( DLGetXMLElementAttributes(triggerXMLElement, edgeTrigAttr, NumElem(edgeTrigAttr)) );
			taskTrig->slope = (TrigSlope_type) slopeType;
			errChk( DLGetXMLElementAttributes(triggerXMLElement, levelTrigAttr, NumElem(levelTrigAttr)) );
			break;
			
		case Trig_AnalogWindow:
			
			errChk( DLGetXMLElementAttributes(triggerXMLElement, windowTrigAttr, NumElem(windowTrigAttr)) );
			taskTrig->wndTrigCond	= (TrigWndCond_type) triggerCondition; 
			break;
	}
		
	
	return 0;
	
Error:
	
	return errorInfo.error;
}

static int LoadChannelCfg (Dev_type* dev, ChanSet_type** chanSetPtr, ActiveXMLObj_IXMLDOMElement_ channelXMLElement, ERRORINFO* xmlErrorInfo)
{
#define LoadChannelCfg_Err_NotImplemented		-1
	
INIT_ERR
	
	
	uInt32							chanType					= 0;
	char*							physChanName				= NULL;
	BOOL							onDemand					= FALSE;
	ActiveXMLObj_IXMLDOMElement_	triggersXMLElement			= 0;	 // holds the "Triggers" element
	ActiveXMLObj_IXMLDOMElement_	startTriggerXMLElement		= 0;	 // holds the "StartTrigger" element
	ActiveXMLObj_IXMLDOMElement_	referenceTriggerXMLElement	= 0;	 // holds the "ReferenceTrigger" element
	
	DAQLabXMLNode 					SharedChanAttr[]			= { {"PhysicalChannelName", 	BasicData_CString, 		&physChanName},
																	{"ChannelType",				BasicData_UInt, 		&chanType},
																	{"OnDemand",				BasicData_Bool,			&onDemand} };
	
	
	// load common channel attributes
	errChk( DLGetXMLElementAttributes(channelXMLElement, SharedChanAttr, NumElem(SharedChanAttr)) ); 
	
	// load channel specific attributes
	switch ((Channel_type)chanType) {
			
		case Chan_AI_Voltage:
			{
				AIChannel_type*					AIChanAttr 				= GetAIChannel(dev, physChanName); 
				unsigned int					VRangeIdx				= 0;
				double							VMax					= 0;
				double							VMin					= 0;
				BOOL							integrateFlag			= FALSE;
				uInt32							terminalType			= 0;
				uInt32							AIDataTypeConversion	= 0;
				double							dataTypeMin				= 0;
				double							dataTypeMax				= 0;
				double							dataTypeGain			= 0;
				double							dataTypeOffset			= 0;
				DAQLabXMLNode 					ChanAIVoltageAttr[]		= { {"VRangeIdx",			BasicData_UInt,		&VRangeIdx},
																			{"VMax", 				BasicData_Double, 	&VMax},
																			{"VMin",				BasicData_Double, 	&VMin},
																			{"Terminal",			BasicData_UInt,		&terminalType},
																			{"DataTypeConversion", 	BasicData_UInt,		&AIDataTypeConversion},
																			{"DataTypeMin",			BasicData_Double,	&dataTypeMin}, 
																			{"DataTypeMax",			BasicData_Double,	&dataTypeMax}, 
																			{"DataTypeGain",		BasicData_Double,	&dataTypeGain},
																			{"DataTypeOffset",		BasicData_Double,	&dataTypeOffset},
																			{"Integrate",			BasicData_Bool,		&integrateFlag} };
																			
																			
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanAIVoltageAttr, NumElem(ChanAIVoltageAttr)) ); 
				
				DataTypeConversion_type			dataTypeConversion		= {.dataType = (DataTypeConversions)AIDataTypeConversion, .min = dataTypeMin, .max = dataTypeMax, .offset = dataTypeOffset, .gain = dataTypeGain};
				
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_AI_Voltage_type(dev, physChanName, AIChanAttr, integrateFlag, dataTypeConversion, VRangeIdx, (Terminal_type) terminalType) );
				// reserve channel
				AIChanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_AI_Current:
			{
				AIChannel_type*					chanAttr 				= GetAIChannel(dev, physChanName);
				unsigned int					IRangeIdx				= 0;
				double							IMax					= 0;
				double							IMin					= 0;
				BOOL							integrateFlag			= FALSE;
				uInt32							terminalType			= 0;
				uInt32							AIDataTypeConversion	= 0;
				uInt32							AIShuntResistorType		= 0;
				double							shuntResistor			= 0;
				double							dataTypeMin				= 0;
				double							dataTypeMax				= 0;
				double							dataTypeGain			= 0;
				double							dataTypeOffset			= 0;
				DAQLabXMLNode 					ChanAICurrentAttr[]		= { {"IRangeIdx",			BasicData_UInt,		&IRangeIdx},
																			{"IMax", 				BasicData_Double, 	&IMax},
																			{"IMin",				BasicData_Double, 	&IMin},
																			{"Terminal",			BasicData_UInt,		&terminalType},
																			{"ShuntResistorType",	BasicData_UInt,		&AIShuntResistorType},
																			{"ShuntResistorValue",	BasicData_Double,	&shuntResistor},
																			{"DataTypeConversion", 	BasicData_UInt,		&AIDataTypeConversion},
																			{"DataTypeMin",			BasicData_Double,	&dataTypeMin}, 
																			{"DataTypeMax",			BasicData_Double,	&dataTypeMax}, 
																			{"DataTypeGain",		BasicData_Double,	&dataTypeGain},
																			{"DataTypeOfset",		BasicData_Double,	&dataTypeOffset},
																			{"Integrate",			BasicData_Double,	&integrateFlag} };
																			
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanAICurrentAttr, NumElem(ChanAICurrentAttr)) ); 
				
				DataTypeConversion_type			dataTypeConversion		= {.dataType = (DataTypeConversions)AIDataTypeConversion, .min = dataTypeMin, .max = dataTypeMax, .offset = dataTypeOffset, .gain = dataTypeGain};
				
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_AI_Current_type(dev, physChanName, chanAttr, integrateFlag, dataTypeConversion, IRangeIdx, (ShuntResistor_type) AIShuntResistorType, shuntResistor, (Terminal_type) terminalType) );
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_AO_Voltage:
			{
				AOChannel_type*					chanAttr 				= GetAOChannel(dev, physChanName); 
				uInt32							terminalType			= 0;
				unsigned int					VRangeIdx				= 0;
				double							VMax					= 0;
				double							VMin					= 0;
				//DataTypeConversion_type			dataTypeConversion		= {.dataType = Convert_To_Double, .min = 0, .max = 0, .offset = 0, .gain = 0};
				DAQLabXMLNode 					ChanAOVoltageAttr[]		= { {"VRangeIdx",			BasicData_UInt,		&VRangeIdx},
																			{"VMax",				BasicData_Double, 	&VMax},
																			{"VMin",				BasicData_Double, 	&VMin},
																			{"Terminal",			BasicData_UInt,		&terminalType} };
				
																			
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanAOVoltageAttr, NumElem(ChanAOVoltageAttr)) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_AO_Voltage_type(dev, physChanName, chanAttr, VRangeIdx, (Terminal_type) terminalType) );
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_AO_Current:
			{
				AOChannel_type*					chanAttr 				= GetAOChannel(dev, physChanName); 
				uInt32							terminalType			= 0;
				unsigned int					IRangeIdx				= 0;
				double							IMax					= 0;
				double							IMin					= 0;
				//DataTypeConversion_type			dataTypeConversion		= {.dataType = Convert_To_Double, .min = 0, .max = 0, .offset = 0, .gain = 0};
				DAQLabXMLNode 					ChanAOCurrentAttr[]		= { {"IRangeIdx",			BasicData_UInt,		&IRangeIdx},
																			{"IMax",				BasicData_Double, 	&IMax},
																			{"IMin",				BasicData_Double, 	&IMin},
																			{"Terminal",			BasicData_UInt,		&terminalType} };
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanAOCurrentAttr, NumElem(ChanAOCurrentAttr)) );															
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_AO_Current_type(dev, physChanName, chanAttr, IRangeIdx, 0, 0, (Terminal_type) terminalType) );
				// reserve channel
				chanAttr->inUse = TRUE;
				
			}
			break;
			
		case Chan_DI_Line:
			{
				DILineChannel_type*				chanAttr				= GetDILineChannel(dev, physChanName);
				BOOL							invert					= FALSE;
				DAQLabXMLNode 					ChanDIDOAttr[]			= { {"Invert",				BasicData_Bool, 	&invert} };
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanDIDOAttr, NumElem(ChanDIDOAttr)) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_DIDO_type(dev, physChanName, Chan_DI_Line, invert) );
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_DI_Port:
			{
				DIPortChannel_type*				chanAttr				= GetDIPortChannel(dev, physChanName);
				BOOL							invert					= FALSE;
				DAQLabXMLNode 					ChanDIDOAttr[]			= { {"Invert",				BasicData_Bool, 	&invert} };
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanDIDOAttr, NumElem(ChanDIDOAttr)) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_DIDO_type(dev, physChanName, Chan_DI_Line, invert) );
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_DO_Line:
			{
				DOLineChannel_type*				chanAttr				= GetDOLineChannel(dev, physChanName);
				BOOL							invert					= FALSE;
				DAQLabXMLNode 					ChanDIDOAttr[]			= { {"Invert",				BasicData_Bool, 	&invert} };
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanDIDOAttr, NumElem(ChanDIDOAttr)) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_DIDO_type(dev, physChanName, Chan_DI_Line, invert) );
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_DO_Port:
			{
				DOPortChannel_type*				chanAttr				= GetDOPortChannel(dev, physChanName);
				BOOL							invert					= FALSE;
				DAQLabXMLNode 					ChanDIDOAttr[]			= { {"Invert",				BasicData_Bool, 	&invert} };
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, ChanDIDOAttr, NumElem(ChanDIDOAttr)) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_DIDO_type(dev, physChanName, Chan_DI_Line, invert) );
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_CI_EdgeCount:
			{
				CIChannel_type*					chanAttr				= GetCIChannel(dev, physChanName);
				uInt32							edgeType				= 0;
				uInt32							initialCount			= 0;
				uInt32							countDirection			= 0;
				DAQLabXMLNode 					CIEdgeCountAttr[]		= { {"ActiveEdge",			BasicData_UInt, 	&edgeType},
																			{"InitialCount",		BasicData_UInt,		&initialCount},
																			{"CountDirection",		BasicData_UInt,		&countDirection} };
																			
				errChk( DLGetXMLElementAttributes(channelXMLElement, CIEdgeCountAttr, NumElem(CIEdgeCountAttr)) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_CI_EdgeCount_type(dev, physChanName, edgeType, initialCount, countDirection) );
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
			
		case Chan_CI_Frequency:
			{
				CIChannel_type*					chanAttr				= GetCIChannel(dev, physChanName);
				uInt32							edgeType				= 0;
				uInt32							freqMeasMethod			= 0;
				double							freqMax					= 0;
				double							freqMin					= 0;
				double							measTime				= 0;
				uInt32							divisor					= 0;
				char*							freqInputTerminal		= NULL;
				CounterTaskTiming_type			CIFreqTaskTiming		= {.measMode = 0, .nSamples = 0, .sampleRate = 0, .refClkFreq = 0};
				DAQLabXMLNode 					CIFreqChanAttr[]		= { {"MeasurementMode",				BasicData_UInt,			&CIFreqTaskTiming.measMode},
																			{"NSamples",					BasicData_UInt64,	&CIFreqTaskTiming.nSamples},
																			{"SamplingRate",				BasicData_Double,		&CIFreqTaskTiming.sampleRate},
																			{"ReferenceClockFrequency", 	BasicData_Double,		&CIFreqTaskTiming.refClkFreq},
																			{"MaxFrequency",				BasicData_Double, 		&freqMax},
																			{"MinFrequency",				BasicData_Double, 		&freqMin},
																			{"ActiveEdge",					BasicData_UInt, 		&edgeType},
																			{"FrequencyMeasurementMethod",	BasicData_UInt,			&freqMeasMethod},
																			{"MeasurementTime",				BasicData_Double,		&measTime},
																			{"Divisor",						BasicData_UInt,			&divisor},
																			{"FrequencyInputTerminal",		BasicData_CString,		freqInputTerminal}};
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, CIFreqChanAttr, NumElem(CIFreqChanAttr)) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_CI_Frequency_type(dev, physChanName, freqMax, freqMin, edgeType, freqMeasMethod, measTime, divisor, freqInputTerminal, CIFreqTaskTiming) );
				OKfree(freqInputTerminal);
				// reserve channel
				chanAttr->inUse = TRUE;
			}
			break;
		
		case Chan_CO_Pulse_Time:
			{
				COChannel_type*					chanAttr				= GetCOChannel(dev, physChanName);
				PulseTrainTimeTiming_type*		pulseTrain				= NULL;				
				uInt32							pulseMode				= 0;
				uInt32							idlePulseState			= 0;
				uInt64							nPulses					= 0;
				double							highTime				= 0;
				double							lowTime					= 0;
				double							initialDelay			= 0;
				DAQLabXMLNode 					COChanAttr[]			= { {"PulseMode",				BasicData_UInt, 		&pulseMode},
																			{"IdlePulseState",			BasicData_UInt,			&idlePulseState},
																			{"NPulses",					BasicData_UInt64,		&nPulses},
																			{"PulseHighTime",			BasicData_Double,		&highTime},
																			{"PulseLowTime",			BasicData_Double,		&lowTime},
																			{"PulseInitialDelay",		BasicData_Double,		&initialDelay}};
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, COChanAttr, NumElem(COChanAttr)) );
				nullChk( pulseTrain = init_PulseTrainTimeTiming_type((PulseTrainModes)pulseMode, (PulseTrainIdleStates)idlePulseState, nPulses, highTime, lowTime, initialDelay) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_CO_type(dev, physChanName, (Channel_type) chanType, NULL, (PulseTrain_type*)pulseTrain) );
				
				// reserve channel
				chanAttr->inUse = TRUE;
				
			}
			break;
			
		case Chan_CO_Pulse_Frequency:
			{
				COChannel_type*					chanAttr				= GetCOChannel(dev, physChanName);
				PulseTrainFreqTiming_type*		pulseTrain				= NULL;				
				uInt32							pulseMode				= 0;
				uInt32							idlePulseState			= 0;
				uInt64							nPulses					= 0;
				double							frequency				= 0;
				double							dutyCycle				= 0;
				double							initialDelay			= 0;
				DAQLabXMLNode 					COChanAttr[]			= { {"PulseMode",				BasicData_UInt, 		&pulseMode},
																			{"IdlePulseState",			BasicData_UInt,			&idlePulseState},
																			{"NPulses",					BasicData_UInt64,		&nPulses},
																			{"Frequency",				BasicData_Double,		&frequency},
																			{"DutyCycle",				BasicData_Double,		&dutyCycle},
																			{"InitialDelay",			BasicData_Double,		&initialDelay} };
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, COChanAttr, NumElem(COChanAttr)) );
				nullChk( pulseTrain = init_PulseTrainFreqTiming_type((PulseTrainModes)pulseMode, (PulseTrainIdleStates)idlePulseState, nPulses, dutyCycle, frequency, initialDelay) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_CO_type(dev, physChanName, (Channel_type) chanType, NULL, (PulseTrain_type*)pulseTrain) );
				// reserve channel
				chanAttr->inUse = TRUE;
				
			}
			break;
			
		case Chan_CO_Pulse_Ticks:
			{
				COChannel_type*					chanAttr				= GetCOChannel(dev, physChanName);
				PulseTrainTickTiming_type*		pulseTrain				= NULL;				
				uInt32							pulseMode				= 0;
				uInt32							idlePulseState			= 0;
				uInt64							nPulses					= 0;
				char*							clockSource				= NULL;
				uInt32							highTicks				= 0;
				uInt32							lowTicks				= 0;
				uInt32							delayTicks				= 0;
				
				DAQLabXMLNode 					COChanAttr[]			= { {"PulseMode",				BasicData_UInt, 		&pulseMode},
																			{"IdlePulseState",			BasicData_UInt,			&idlePulseState},
																			{"NPulses",					BasicData_UInt64,		&nPulses},
																			{"ClockSource", 			BasicData_CString,		&clockSource},
																			{"HighTicks",				BasicData_UInt,			&highTicks},
																			{"LowTicks",				BasicData_UInt,			&lowTicks},
																			{"DelayTicks",				BasicData_UInt,			&delayTicks} };
				
				errChk( DLGetXMLElementAttributes(channelXMLElement, COChanAttr, NumElem(COChanAttr)) );
				nullChk( pulseTrain = init_PulseTrainTickTiming_type((PulseTrainModes)pulseMode, (PulseTrainIdleStates)idlePulseState, nPulses, highTicks, lowTicks, delayTicks) );
				nullChk( *chanSetPtr = (ChanSet_type*) init_ChanSet_CO_type(dev, physChanName, (Channel_type) chanType, clockSource, (PulseTrain_type*)pulseTrain) );
				OKfree(clockSource);
				// reserve channel
				chanAttr->inUse = TRUE;
				
			}
			break;
			
		default:
			
			return LoadChannelCfg_Err_NotImplemented;
	}
	
	
	// get triggers XML element
	errChk ( DLGetSingleXMLElementFromElement(channelXMLElement, "Triggers", &triggersXMLElement) );
	
	// get start trigger XML element
	if (triggersXMLElement)
		errChk ( DLGetSingleXMLElementFromElement(triggersXMLElement, "StartTrigger", &startTriggerXMLElement) );
	
	// get reference trigger XML element
	if (triggersXMLElement)
		errChk ( DLGetSingleXMLElementFromElement(triggersXMLElement, "ReferenceTrigger", &referenceTriggerXMLElement) );
	
	// load trigger attributes only for counter channels
	switch ((*chanSetPtr)->chanType) {
			
		case Chan_CI_EdgeCount:
		case Chan_CI_Frequency:
			
			// load start trigger settings
			if (startTriggerXMLElement) {
				nullChk( ((ChanSet_CI_type*)*chanSetPtr)->startTrig  	= init_TaskTrig_type(dev, NULL) ); 		   // for now there's no sample rate associated with counter type trigger
				errChk( LoadTaskTrigCfg(((ChanSet_CI_type*)*chanSetPtr)->startTrig, startTriggerXMLElement, xmlErrorInfo) );
			}
			
			// load start trigger settings
			if (referenceTriggerXMLElement) {
				nullChk( ((ChanSet_CI_type*)*chanSetPtr)->referenceTrig  = init_TaskTrig_type(dev, NULL) );			// for now there's no sample rate associated with counter type trigger
				errChk( LoadTaskTrigCfg(((ChanSet_CI_type*)*chanSetPtr)->startTrig, referenceTriggerXMLElement, xmlErrorInfo) );
		
			}
			break;
			
		case Chan_CO_Pulse_Time:
		case Chan_CO_Pulse_Frequency:
		case Chan_CO_Pulse_Ticks:
			
			// load start trigger settings
			if (startTriggerXMLElement) {
				nullChk( ((ChanSet_CO_type*)*chanSetPtr)->startTrig  	= init_TaskTrig_type(dev, NULL) ); 			// for now there's no sample rate associated with counter type trigger
				errChk( LoadTaskTrigCfg(((ChanSet_CO_type*)*chanSetPtr)->startTrig, startTriggerXMLElement, xmlErrorInfo) );
			}
			
			// load reference trigger settings
			if (referenceTriggerXMLElement) {
				nullChk( ((ChanSet_CO_type*)*chanSetPtr)->referenceTrig  = init_TaskTrig_type(dev, NULL) );			// for now there's no sample rate associated with counter type trigger
				errChk( LoadTaskTrigCfg(((ChanSet_CO_type*)*chanSetPtr)->startTrig, referenceTriggerXMLElement, xmlErrorInfo) );
		
			}
			break;
			
		default:
			
			break;
	}
	
	// cleanup
	OKfree(physChanName);
	OKfreeCAHndl(startTriggerXMLElement);
	OKfreeCAHndl(referenceTriggerXMLElement);
	
	return 0;
	
Error:
	
	return errorInfo.error;

}

static int SaveCfg (DAQLabModule_type* mod, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_  moduleElement, ERRORINFO* xmlErrorInfo)
{
INIT_ERR
	
	NIDAQmxManager_type*			NIDAQ	 				= (NIDAQmxManager_type*) mod;
	
	//--------------------------------------------------------------------------
	// Save DAQmanager main panel position
	//--------------------------------------------------------------------------
	
	int*								panTopPos			= NULL;
	int*								panLeftPos			= NULL;
	nullChk( panTopPos 		= malloc(sizeof(int)) );
	nullChk( panLeftPos 	= malloc(sizeof(int)) );				 
	DAQLabXMLNode 					NIDAQAttr[] 			= {	{"PanTopPos", BasicData_Int, panTopPos},
											  		   			{"PanLeftPos", BasicData_Int, panLeftPos}	};
	
	
	errChk( GetPanelAttribute(NIDAQ->mainPanHndl, ATTR_LEFT, panLeftPos) );
	errChk( GetPanelAttribute(NIDAQ->mainPanHndl, ATTR_TOP, panTopPos) );
	errChk( DLAddToXMLElem(xmlDOM, moduleElement, NIDAQAttr, DL_ATTRIBUTE, NumElem(NIDAQAttr), xmlErrorInfo) ); 
	
	//--------------------------------------------------------------------------
	// Save NIDAQ device configuration
	//--------------------------------------------------------------------------
	
	size_t							nDevices				= ListNumItems(NIDAQ->DAQmxDevices);
	Dev_type*						dev						= NULL;
	ActiveXMLObj_IXMLDOMElement_	NIDAQDeviceXMLElement	= 0;	 // holds an "NIDAQDevice" element to which device settings are added
	
	for (size_t i = 1; i <= nDevices; i++) {
		dev = *(Dev_type**)ListGetPtrToItem(NIDAQ->DAQmxDevices, i);
		// create new device XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "NIDAQDevice", &NIDAQDeviceXMLElement) );
		// save device config
		errChk( SaveDeviceCfg(dev, xmlDOM, NIDAQDeviceXMLElement, xmlErrorInfo) );
		// add device XML element to NIDAQ module
		errChk ( ActiveXML_IXMLDOMElement_appendChild (moduleElement, xmlErrorInfo, NIDAQDeviceXMLElement, NULL) );
		// discard device XML element handle
		OKfreeCAHndl(NIDAQDeviceXMLElement); 
	}
	
	OKfree(panTopPos); 
	OKfree(panLeftPos);
	return 0;
	
Error:
	
	return errorInfo.error;
}

static int SaveDeviceCfg (Dev_type* dev, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ NIDAQDeviceXMLElement, ERRORINFO* xmlErrorInfo) 
{
INIT_ERR	
	
	//--------------------------------------------------------------------------------
	// Save  	- device product ID that must match when the settings are loaded again
	//			- task controller name
	//--------------------------------------------------------------------------------
	
	char*							tcName					= GetTaskControlName(dev->taskController);
	DAQLabXMLNode 					DevAttr[] 				= {	{"NIProductClassID", 	BasicData_UInt, 	&dev->attr->productNum},
																{"DeviceSerialNumber",	BasicData_UInt,		&dev->attr->serial},
																{"Name", 				BasicData_CString, 	tcName}	};
	
	errChk( DLAddToXMLElem(xmlDOM, NIDAQDeviceXMLElement, DevAttr, DL_ATTRIBUTE, NumElem(DevAttr), xmlErrorInfo) ); 
	OKfree(tcName);
	
	//--------------------------------------------------------------------------------
	// Save DAQmx tasks
	//--------------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMElement_	AITaskXMLElement	= 0;	 // holds an "AITask" element
	ActiveXMLObj_IXMLDOMElement_	AOTaskXMLElement	= 0;	 // holds an "AOTask" element
	//ActiveXMLObj_IXMLDOMElement_	DITaskXMLElement	= 0;	 // holds an "DITask" element
	//ActiveXMLObj_IXMLDOMElement_	DOTaskXMLElement	= 0;	 // holds an "DOTask" element
	//ActiveXMLObj_IXMLDOMElement_	CITaskXMLElement	= 0;	 // holds an "CITask" element
	//ActiveXMLObj_IXMLDOMElement_	COTaskXMLElement	= 0;	 // holds an "COTask" element
	
	// AI task
	if (dev->AITaskSet) {
		// create new task XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "AITask", &AITaskXMLElement) );
		// save task config
		errChk( SaveADTaskCfg (dev->AITaskSet, xmlDOM, AITaskXMLElement, xmlErrorInfo) );
		// add task XML element to device element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (NIDAQDeviceXMLElement, xmlErrorInfo, AITaskXMLElement, NULL) );
		// discard task XML element handle
		OKfreeCAHndl(AITaskXMLElement);
	}
	
	// AO task
	if (dev->AOTaskSet) {
		// create new task XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "AOTask", &AOTaskXMLElement) );
		// save task config
		errChk( SaveADTaskCfg (dev->AOTaskSet, xmlDOM, AOTaskXMLElement, xmlErrorInfo) );
		// add device XML element to device element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (NIDAQDeviceXMLElement, xmlErrorInfo, AOTaskXMLElement, NULL) );
		// discard task XML element handle
		OKfreeCAHndl(AOTaskXMLElement);
	}
	
	return 0;
	
Error:
	
	return errorInfo.error;
}
		
static int SaveADTaskCfg (ADTaskSet_type* taskSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement, ERRORINFO* xmlErrorInfo) 
{
INIT_ERR
	
	//--------------------------------------------------------------------------------
	// Save taskSet properties
	//--------------------------------------------------------------------------------
	
	uInt32							operationMode			= (uInt32)taskSet->timing->measMode;
	uInt32							sampleClockEdge			= (uInt32)taskSet->timing->sampClkEdge;
	
	DAQLabXMLNode 					taskSetAttr[] 				= {	{"Timeout", 					BasicData_Double, 		&taskSet->timeout},
																	{"OperationMode", 				BasicData_UInt, 		&operationMode},
																	{"SamplingRate", 				BasicData_Double, 		&taskSet->timing->sampleRate},
																	{"TargetSamplingRate",			BasicData_Double,		&taskSet->timing->targetSampleRate},
																	{"NSamples", 					BasicData_UInt64, 		&taskSet->timing->nSamples},
																	{"Oversampling",				BasicData_UInt,			&taskSet->timing->oversampling},
																	{"OversamplingAutoAdjust",		BasicData_Bool,			&taskSet->timing->oversamplingAuto},
																	{"BlockSize", 					BasicData_UInt, 		&taskSet->timing->blockSize},
																	{"SampleClockSource", 			BasicData_CString, 		taskSet->timing->sampClkSource},
																	{"SampleClockEdge", 			BasicData_UInt, 		&sampleClockEdge},
																	{"ReferenceClockSource", 		BasicData_CString, 		taskSet->timing->refClkSource},
																	{"ReferenceClockFrequency", 	BasicData_Double, 		&taskSet->timing->refClkFreq},
																	{"StartSignalRouting",			BasicData_CString,		taskSet->timing->startSignalRouting} };
																	
	errChk( DLAddToXMLElem(xmlDOM, AITaskXMLElement, taskSetAttr, DL_ATTRIBUTE, NumElem(taskSetAttr), xmlErrorInfo) ); 
	
	//--------------------------------------------------------------------------------
	// Save taskSet triggers
	//--------------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMElement_	TriggersXMLElement			= 0;	 // holds the "Triggers" element
	ActiveXMLObj_IXMLDOMElement_	StartTriggerXMLElement		= 0;	 // holds the "StartTrigger" element
	ActiveXMLObj_IXMLDOMElement_	ReferenceTriggerXMLElement	= 0;	 // holds the "ReferenceTrigger" element
	
	// create new triggers XML element
	if (taskSet->startTrig || taskSet->referenceTrig)
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Triggers", &TriggersXMLElement) );
	
	//----------------------
	// Start Trigger
	//----------------------
	if (taskSet->startTrig) {
		// create "StartTrigger" XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "StartTrigger", &StartTriggerXMLElement) );
		// save trigger info
		errChk( SaveTaskTrigCfg(taskSet->startTrig, xmlDOM, StartTriggerXMLElement, xmlErrorInfo) ); 
		// add "StartTrigger" XML element to "Triggers" XML element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (TriggersXMLElement, xmlErrorInfo, StartTriggerXMLElement, NULL) );
		// discard "StartTrigger" XML element handle
		OKfreeCAHndl(StartTriggerXMLElement);
	}
	
	//----------------------
	// Reference Trigger
	//----------------------
	if (taskSet->referenceTrig) {
		// create "ReferenceTrigger" XML element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ReferenceTrigger", &ReferenceTriggerXMLElement) );
		// save trigger info
		errChk( SaveTaskTrigCfg(taskSet->referenceTrig, xmlDOM, ReferenceTriggerXMLElement, xmlErrorInfo) );
		DAQLabXMLNode 	refTrigAttr[] = {{"NPreTrigSamples", BasicData_UInt, &taskSet->referenceTrig->nPreTrigSamples}};
		errChk( DLAddToXMLElem(xmlDOM, ReferenceTriggerXMLElement, refTrigAttr, DL_ATTRIBUTE, NumElem(refTrigAttr), xmlErrorInfo) ); 
		// add "ReferenceTrigger" XML element to "Triggers" XML element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (TriggersXMLElement, xmlErrorInfo, ReferenceTriggerXMLElement, NULL) );
		// discard "ReferenceTrigger" XML element handle
		OKfreeCAHndl(ReferenceTriggerXMLElement);
	}
	
	// add "Triggers" XML element to taskSet element
	if (taskSet->startTrig || taskSet->referenceTrig) {
		errChk ( ActiveXML_IXMLDOMElement_appendChild (AITaskXMLElement, xmlErrorInfo, TriggersXMLElement, NULL) );
		// discard "Triggers" XML element handle
		OKfreeCAHndl(TriggersXMLElement);
	}
	
	//--------------------------------------------------------------------------------
	// Save taskSet channels
	//--------------------------------------------------------------------------------
	
	ActiveXMLObj_IXMLDOMElement_	ChannelsXMLElement			= 0;	 // holds the "Channels" element
	ActiveXMLObj_IXMLDOMElement_	ChannelXMLElement			= 0;	 // holds the "Channel" element for each channel in the taskSet
	size_t							nChannels					= ListNumItems(taskSet->chanSet);
	ChanSet_type*					chanSet						= NULL;
	
	// create new "Channels" XML element
	errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Channels", &ChannelsXMLElement) );
	// save channels info
	for (size_t i = 1; i <= nChannels; i++) {
		chanSet = *(ChanSet_type**) ListGetPtrToItem(taskSet->chanSet, i);
		// create new "Channel" element
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Channel", &ChannelXMLElement) );
		// save channel info
		errChk( SaveChannelCfg(chanSet, xmlDOM, ChannelXMLElement, xmlErrorInfo) );
		// add "Channel" element to "Channels" element
		errChk ( ActiveXML_IXMLDOMElement_appendChild (ChannelsXMLElement, xmlErrorInfo, ChannelXMLElement, NULL) );
		// discard "Channel" XML element handle
		OKfreeCAHndl(ChannelXMLElement);
	}
	
	// add "Channels" XML element to taskSet element
	errChk ( ActiveXML_IXMLDOMElement_appendChild (AITaskXMLElement, xmlErrorInfo, ChannelsXMLElement, NULL) );
	// discard "Channels" XML element handle
	OKfreeCAHndl(ChannelsXMLElement);
	
	return 0;
	
Error:
	
	return errorInfo.error;	
}

static int SaveCITaskCfg (CITaskSet_type* taskSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AITaskXMLElement, ERRORINFO* xmlErrorInfo) 
{
//INIT_ERR
	
	
	return 0;
}

static int SaveCOTaskCfg (COTaskSet_type* taskSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ AOTaskXMLElement, ERRORINFO* xmlErrorInfo) 
{
//INIT_ERR
	
	
	return 0;
}

static int SaveTaskTrigCfg (TaskTrig_type* taskTrig, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ TriggerXMLElement, ERRORINFO* xmlErrorInfo)
{
#define SaveTaskTrigCfg_Err_NotImplemented		-1
	
INIT_ERR	
	
	uInt32							trigType				= (uInt32) taskTrig->trigType;
	uInt32							slopeType				= (uInt32) taskTrig->slope;
	uInt32							triggerCondition		= (uInt32) taskTrig->wndTrigCond;
		// shared trigger attributes
	DAQLabXMLNode 					sharedTrigAttr[]		= {	{"Type", 					BasicData_UInt, 	&trigType},
																{"Source", 					BasicData_CString, 	taskTrig->trigSource} };
		// for both analog and digital edge triggers
	DAQLabXMLNode 					edgeTrigAttr[]			= {	{"Slope", 					BasicData_UInt, 	&slopeType} };
		// for analog edge trigger
	DAQLabXMLNode 					levelTrigAttr[]			= {	{"Level", 					BasicData_Double, 	&taskTrig->level} };
		// for analog window trigger
	DAQLabXMLNode 					windowTrigAttr[]		= {	{"WindowTop", 				BasicData_Double, 	&taskTrig->wndTop},
																{"WindowBottom", 			BasicData_Double, 	&taskTrig->wndBttm},
																{"WindowTriggerCondition", 	BasicData_UInt, 	&triggerCondition} };

	// add shared trigger attributes 
	errChk( DLAddToXMLElem(xmlDOM, TriggerXMLElement, sharedTrigAttr, DL_ATTRIBUTE, NumElem(sharedTrigAttr), xmlErrorInfo) ); 
	
	// add specific trigger attributes
	switch (taskTrig->trigType) {
			
		case Trig_None:
			
			break;
			
		case Trig_DigitalEdge:
			
			errChk( DLAddToXMLElem(xmlDOM, TriggerXMLElement, edgeTrigAttr, DL_ATTRIBUTE, NumElem(edgeTrigAttr), xmlErrorInfo) ); 
			
			break;
			
		case Trig_DigitalPattern:
			
			return SaveTaskTrigCfg_Err_NotImplemented;
			
		case Trig_AnalogEdge:
			
			errChk( DLAddToXMLElem(xmlDOM, TriggerXMLElement, edgeTrigAttr, DL_ATTRIBUTE, NumElem(edgeTrigAttr), xmlErrorInfo) ); 
			errChk( DLAddToXMLElem(xmlDOM, TriggerXMLElement, levelTrigAttr, DL_ATTRIBUTE, NumElem(levelTrigAttr), xmlErrorInfo) ); 
			
			break;
			
		case Trig_AnalogWindow:
			
			errChk( DLAddToXMLElem(xmlDOM, TriggerXMLElement, windowTrigAttr, DL_ATTRIBUTE, NumElem(windowTrigAttr), xmlErrorInfo) ); 
	}
									
	
	return 0;
	
Error:
	
	return errorInfo.error;
}

static int SaveChannelCfg (ChanSet_type* chanSet, CAObjHandle xmlDOM, ActiveXMLObj_IXMLDOMElement_ ChannelXMLElement, ERRORINFO* xmlErrorInfo)
{
#define SaveChannelCfg_Err_NotImplemented		-1

INIT_ERR
	
	uInt32							chanType					= (uInt32) chanSet->chanType;
	DAQLabXMLNode 					SharedChanAttr[]			= { {"PhysicalChannelName", 	BasicData_CString, 		chanSet->name},
																	{"ChannelType",				BasicData_UInt, 		&chanType},
																	{"OnDemand",				BasicData_Bool,			&chanSet->onDemand} };
	ActiveXMLObj_IXMLDOMElement_	TriggersXMLElement			= 0;	 // holds the "Triggers" element
	ActiveXMLObj_IXMLDOMElement_	StartTriggerXMLElement		= 0;	 // holds the "StartTrigger" element
	ActiveXMLObj_IXMLDOMElement_	ReferenceTriggerXMLElement	= 0;	 // holds the "ReferenceTrigger" element
	
	
	// add common channel attributes
	errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, SharedChanAttr, DL_ATTRIBUTE, NumElem(SharedChanAttr), xmlErrorInfo) );
	
	// add channel specific attributes
	switch (chanSet->chanType) {
			
		case Chan_AI_Voltage:
			{
				ChanSet_AI_Voltage_type*		AIVoltageChanSet		= (ChanSet_AI_Voltage_type*) chanSet;
				uInt32							AIVoltageTerminalType	= (uInt32) AIVoltageChanSet->terminal;
				uInt32							AIDataTypeConversion	= (uInt32) AIVoltageChanSet->baseClass.dataTypeConversion.dataType;
			
				DAQLabXMLNode 					ChanAIVoltageAttr[]		= { {"VRangeIdx",			BasicData_UInt,		&((AIChannel_type*)AIVoltageChanSet->chanAttr)->Vrngs->selectedRange},
																			{"VMax", 				BasicData_Double, 	&AIVoltageChanSet->VMax},
																			{"VMin",				BasicData_Double, 	&AIVoltageChanSet->VMin},
																			{"Terminal",			BasicData_UInt,		&AIVoltageTerminalType},
																			{"DataTypeConversion", 	BasicData_UInt,		&AIDataTypeConversion},
																			{"DataTypeMin",			BasicData_Double,	&AIVoltageChanSet->baseClass.dataTypeConversion.min}, 
																			{"DataTypeMax",			BasicData_Double,	&AIVoltageChanSet->baseClass.dataTypeConversion.max}, 
																			{"DataTypeGain",		BasicData_Double,	&AIVoltageChanSet->baseClass.dataTypeConversion.gain},
																			{"DataTypeOffset",		BasicData_Double,	&AIVoltageChanSet->baseClass.dataTypeConversion.offset},
																			{"Integrate",			BasicData_Bool,		&AIVoltageChanSet->baseClass.integrateFlag} };
																			
																			
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, ChanAIVoltageAttr, DL_ATTRIBUTE, NumElem(ChanAIVoltageAttr), xmlErrorInfo) );
			}
			break;
			
		case Chan_AI_Current:
			{
				ChanSet_AI_Current_type*		AICurrentChanSet		= (ChanSet_AI_Current_type*) chanSet;
				uInt32							AICurrentTerminalType	= (uInt32) AICurrentChanSet->terminal;
				uInt32							AIShuntResistorType		= (uInt32) AICurrentChanSet->shuntResistorType;
				uInt32							AIDataTypeConversion	= (uInt32) AICurrentChanSet->baseClass.dataTypeConversion.dataType;
			
				DAQLabXMLNode 					ChanAICurrentAttr[]		= { {"IRangeIdx",			BasicData_UInt,		&((AIChannel_type*)AICurrentChanSet->chanAttr)->Irngs->selectedRange},
																			{"IMax", 				BasicData_Double, 	&AICurrentChanSet->IMax},
																			{"IMin",				BasicData_Double, 	&AICurrentChanSet->IMin},
																			{"Terminal",			BasicData_UInt,		&AICurrentTerminalType},
																			{"ShuntResistorType",	BasicData_UInt,		&AIShuntResistorType},
																			{"ShuntResistorValue",	BasicData_Double,	&AICurrentChanSet->shuntResistorValue},
																			{"DataTypeConversion", 	BasicData_UInt,		&AIDataTypeConversion},
																			{"DataTypeMin",			BasicData_Double,	&AICurrentChanSet->baseClass.dataTypeConversion.min}, 
																			{"DataTypeMax",			BasicData_Double,	&AICurrentChanSet->baseClass.dataTypeConversion.max}, 
																			{"DataTypeGain",		BasicData_Double,	&AICurrentChanSet->baseClass.dataTypeConversion.gain},
																			{"DataTypeOfset",		BasicData_Double,	&AICurrentChanSet->baseClass.dataTypeConversion.offset},
																			{"Integrate",			BasicData_Double,	&AICurrentChanSet->baseClass.integrateFlag} };
																			
																			
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, ChanAICurrentAttr, DL_ATTRIBUTE, NumElem(ChanAICurrentAttr), xmlErrorInfo) );
			}
			break;
			
		case Chan_AO_Voltage:
			{
				ChanSet_AO_Voltage_type*		AOVoltageChanSet		= (ChanSet_AO_Voltage_type*) chanSet;
				uInt32							AOVoltageTerminalType	= (uInt32) AOVoltageChanSet->terminal;
			
				DAQLabXMLNode 					ChanAOVoltageAttr[]		= { {"VRangeIdx",			BasicData_UInt,		&((AOChannel_type*)AOVoltageChanSet->chanAttr)->Vrngs->selectedRange},
																			{"VMax",				BasicData_Double, 	&AOVoltageChanSet->VMax},
																			{"VMin",				BasicData_Double, 	&AOVoltageChanSet->VMin},
																			{"Terminal",			BasicData_UInt,		&AOVoltageTerminalType} };
				
																			
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, ChanAOVoltageAttr, DL_ATTRIBUTE, NumElem(ChanAOVoltageAttr), xmlErrorInfo) );															
			}
			break;
			
		case Chan_AO_Current:
			{
				ChanSet_AO_Current_type*		AOCurrentChanSet		= (ChanSet_AO_Current_type*) chanSet;
				uInt32							AOCurrentTerminalType	= (uInt32) AOCurrentChanSet->terminal;
			
				DAQLabXMLNode 					ChanAOCurrentAttr[]		= { {"IRangeIdx",			BasicData_UInt,		&((AOChannel_type*)AOCurrentChanSet->chanAttr)->Irngs->selectedRange},
																			{"IMax",				BasicData_Double, 	&AOCurrentChanSet->IMax},
																			{"IMin",				BasicData_Double, 	&AOCurrentChanSet->IMin},
																			{"Terminal",			BasicData_UInt,		&AOCurrentTerminalType} };
				
																			
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, ChanAOCurrentAttr, DL_ATTRIBUTE, NumElem(ChanAOCurrentAttr), xmlErrorInfo) );
				
			}
			break;
			
		case Chan_DI_Line:
		case Chan_DI_Port:
		case Chan_DO_Line:
		case Chan_DO_Port:
			{
				ChanSet_DIDO_type*				DIDOChanSet				= (ChanSet_DIDO_type*) chanSet;
				DAQLabXMLNode 					ChanDIDOAttr[]			= { {"Invert",				BasicData_Bool, 	&DIDOChanSet->invert} };
				
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, ChanDIDOAttr, DL_ATTRIBUTE, NumElem(ChanDIDOAttr), xmlErrorInfo) );
			}
			break;
			
		case Chan_CI_EdgeCount:
			{
				ChanSet_CI_EdgeCount_type*		CIEdgeCountChanSet		= (ChanSet_CI_EdgeCount_type*) chanSet;
				uInt32							CIEdgeType				= (uInt32) CIEdgeCountChanSet->baseClass.activeEdge;
				uInt32							CICountDirection		= (uInt32) CIEdgeCountChanSet->countDirection;
				DAQLabXMLNode 					CIEdgeCountAttr[]		= { {"ActiveEdge",			BasicData_UInt, 	&CIEdgeType},
																			{"InitialCount",		BasicData_UInt,		&CIEdgeCountChanSet->initialCount},
																			{"CountDirection",		BasicData_UInt,		&CICountDirection} };
																			
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, CIEdgeCountAttr, DL_ATTRIBUTE, NumElem(CIEdgeCountAttr), xmlErrorInfo) );
			}
			break;
			
		case Chan_CI_Frequency:
			{
				ChanSet_CI_Frequency_type*		CIFreqChanSet			= (ChanSet_CI_Frequency_type*) chanSet;
				uInt32							CIActiveEdge			= (uInt32) CIFreqChanSet->baseClass.activeEdge;
				uInt32							CIMeasMode				= (uInt32) CIFreqChanSet->taskTiming.measMode;
				uInt32							CIFreqMeasMethod		= (uInt32) CIFreqChanSet->measMethod;
				DAQLabXMLNode 					CIFreqChanAttr[]		= { {"MeasurementMode",				BasicData_UInt,			&CIMeasMode},
																			{"NSamples",					BasicData_UInt64,		&CIFreqChanSet->taskTiming.nSamples},
																			{"SamplingRate",				BasicData_Double,		&CIFreqChanSet->taskTiming.sampleRate},
																			{"ReferenceClockFrequency", 	BasicData_Double,		&CIFreqChanSet->taskTiming.refClkFreq},
																			{"MaxFrequency",				BasicData_Double, 		&CIFreqChanSet->freqMax},
																			{"MinFrequency",				BasicData_Double, 		&CIFreqChanSet->freqMin},
																			{"ActiveEdge",					BasicData_UInt, 		&CIActiveEdge},
																			{"FrequencyMeasurementMethod",	BasicData_UInt,			&CIFreqMeasMethod},
																			{"MeasurementTime",				BasicData_Double,		&CIFreqChanSet->measTime},
																			{"Divisor",						BasicData_UInt,			&CIFreqChanSet->divisor},
																			{"FrequencyInputTerminal",		BasicData_CString,		CIFreqChanSet->freqInputTerminal}};
				
				
				
			}
			break;
		
		case Chan_CO_Pulse_Time:
			{  
				ChanSet_CO_type*				COChanSet				= (ChanSet_CO_type*) chanSet;
				uInt32							pulseMode				= (uInt32) GetPulseTrainMode(COChanSet->pulseTrain);
				uInt32							idlePulseState			= (uInt32) GetPulseTrainIdleState(COChanSet->pulseTrain);
				uInt64							nPulses					= GetPulseTrainNPulses(COChanSet->pulseTrain);
				double							highTime				= GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)COChanSet->pulseTrain);
				double							lowTime					= GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)COChanSet->pulseTrain);
				double							initialDelay			= GetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)COChanSet->pulseTrain);
				
				DAQLabXMLNode 					COChanAttr[]			= { {"PulseMode",				BasicData_UInt, 		&pulseMode},
																			{"IdlePulseState",			BasicData_UInt,			&idlePulseState},
																			{"NPulses",					BasicData_UInt64,		&nPulses},
																			{"PulseHighTime",			BasicData_Double,		&highTime},
																			{"PulseLowTime",			BasicData_Double,		&lowTime},
																			{"PulseInitialDelay",		BasicData_Double,		&initialDelay} };
				
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, COChanAttr, DL_ATTRIBUTE, NumElem(COChanAttr), xmlErrorInfo) );
			
			}
			break;
			
		case Chan_CO_Pulse_Frequency:
			{
				ChanSet_CO_type*				COChanSet				= (ChanSet_CO_type*) chanSet;
				uInt32							pulseMode				= (uInt32) GetPulseTrainMode(COChanSet->pulseTrain);
				uInt32							idlePulseState			= (uInt32) GetPulseTrainIdleState(COChanSet->pulseTrain);
				uInt64							nPulses					= GetPulseTrainNPulses(COChanSet->pulseTrain);
				double							frequency				= GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)COChanSet->pulseTrain);
				double							dutyCycle				= GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)COChanSet->pulseTrain);
				double							initialDelay			= GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)COChanSet->pulseTrain);
				
				DAQLabXMLNode 					COChanAttr[]			= { {"PulseMode",				BasicData_UInt, 		&pulseMode},
																			{"IdlePulseState",			BasicData_UInt,			&idlePulseState},
																			{"NPulses",					BasicData_UInt64,		&nPulses},
																			{"Frequency",				BasicData_Double,		&frequency},
																			{"DutyCycle",				BasicData_Double,		&dutyCycle},
																			{"InitialDelay",			BasicData_Double,		&initialDelay} };
				
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, COChanAttr, DL_ATTRIBUTE, NumElem(COChanAttr), xmlErrorInfo) );
				
			}
			break;
			
		case Chan_CO_Pulse_Ticks:
			{
				ChanSet_CO_type*				COChanSet				= (ChanSet_CO_type*) chanSet;
				uInt32							pulseMode				= (uInt32) GetPulseTrainMode(COChanSet->pulseTrain);
				uInt32							idlePulseState			= (uInt32) GetPulseTrainIdleState(COChanSet->pulseTrain);
				uInt64							nPulses					= GetPulseTrainNPulses(COChanSet->pulseTrain);
				uInt32							highTicks				= GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)COChanSet->pulseTrain);
				uInt32							lowTicks				= GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)COChanSet->pulseTrain);
				uInt32							delayTicks				= GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)COChanSet->pulseTrain);
				
				DAQLabXMLNode 					COChanAttr[]			= { {"PulseMode",				BasicData_UInt, 		&pulseMode},
																			{"IdlePulseState",			BasicData_UInt,			&idlePulseState},
																			{"NPulses",					BasicData_UInt64,		&nPulses},
																			{"ClockSource", 			BasicData_CString,		&COChanSet->clockSource},
																			{"HighTicks",				BasicData_UInt,			&highTicks},
																			{"LowTicks",				BasicData_UInt,			&lowTicks},
																			{"DelayTicks",				BasicData_UInt,			&delayTicks} };
				
				errChk( DLAddToXMLElem(xmlDOM, ChannelXMLElement, COChanAttr, DL_ATTRIBUTE, NumElem(COChanAttr), xmlErrorInfo) );
				
			}
			break;
			
		default:
			
			return SaveChannelCfg_Err_NotImplemented;
	}
	
	// save trigger attributes only for counter channels
	TaskTrig_type*	startTrig 		= NULL;
	TaskTrig_type*	referenceTrig 	= NULL;
	
	switch (chanSet->chanType) {
			
		case Chan_CI_EdgeCount:
		case Chan_CI_Frequency:
			
			startTrig 		= ((ChanSet_CI_type*)chanSet)->startTrig;
			referenceTrig 	= ((ChanSet_CI_type*)chanSet)->referenceTrig;
			break;
			
		case Chan_CO_Pulse_Time:
		case Chan_CO_Pulse_Frequency:
		case Chan_CO_Pulse_Ticks:
			
			startTrig 		= ((ChanSet_CO_type*)chanSet)->startTrig;
			referenceTrig 	= ((ChanSet_CO_type*)chanSet)->referenceTrig;
			break;
			
		default:
			
			break;
	}
	
	// create new triggers XML element
	if (startTrig || referenceTrig)
		errChk ( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "Triggers", &TriggersXMLElement) );
	
	// start Trigger
	if (startTrig) {
		// create "StartTrigger" XML element
		errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "StartTrigger", &StartTriggerXMLElement) );
		// save trigger info
		errChk( SaveTaskTrigCfg(startTrig, xmlDOM, StartTriggerXMLElement, xmlErrorInfo) ); 
		// add "StartTrigger" XML element to "Triggers" XML element
		errChk( ActiveXML_IXMLDOMElement_appendChild (TriggersXMLElement, xmlErrorInfo, StartTriggerXMLElement, NULL) );
		// discard "StartTrigger" XML element handle
		OKfreeCAHndl(StartTriggerXMLElement);
	}
	
	// reference Trigger
	if (referenceTrig) {
		// create "ReferenceTrigger" XML element
		errChk( ActiveXML_IXMLDOMDocument3_createElement (xmlDOM, xmlErrorInfo, "ReferenceTrigger", &ReferenceTriggerXMLElement) );
		// save trigger info
		errChk( SaveTaskTrigCfg(referenceTrig, xmlDOM, ReferenceTriggerXMLElement, xmlErrorInfo) );
		DAQLabXMLNode 	refTrigAttr[] = {{"NPreTrigSamples", BasicData_UInt, &referenceTrig->nPreTrigSamples}};
		errChk( DLAddToXMLElem(xmlDOM, ReferenceTriggerXMLElement, refTrigAttr, DL_ATTRIBUTE, NumElem(refTrigAttr), xmlErrorInfo) ); 
		// add "ReferenceTrigger" XML element to "Triggers" XML element
		errChk( ActiveXML_IXMLDOMElement_appendChild (TriggersXMLElement, xmlErrorInfo, ReferenceTriggerXMLElement, NULL) );
		// discard "ReferenceTrigger" XML element handle
		OKfreeCAHndl(ReferenceTriggerXMLElement);
	}
	
	// add "Triggers" XML element to channel element
	if (startTrig || referenceTrig) {
		errChk ( ActiveXML_IXMLDOMElement_appendChild (ChannelXMLElement, xmlErrorInfo, TriggersXMLElement, NULL) );
		// discard "Triggers" XML element handle
		OKfreeCAHndl(TriggersXMLElement);
	}
	
	
	return 0;
	
Error:
	
	return errorInfo.error;
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
INIT_ERR

	NIDAQmxManager_type* 	nidaq	= (NIDAQmxManager_type*) mod;
	
	
	if (visibleFlag)
		errChk(DisplayPanel(nidaq->mainPanHndl));  
	else
		errChk(HidePanel(nidaq->mainPanHndl));
	
Error:
	return errorInfo.error;
}

//------------------------------------------------------------------------------
// Dev_type
//------------------------------------------------------------------------------

static Dev_type* init_Dev_type (NIDAQmxManager_type* niDAQModule, DevAttr_type** attrPtr, char taskControllerName[])
{
	Dev_type* dev = malloc (sizeof(Dev_type));
	if (!dev) return NULL;
	
	// init
	dev -> nActiveTasks			= 0;


	dev -> taskController		= NULL;
	
	//-------------------------------------------------------------------------------------------------
	// Task Controller
	//-------------------------------------------------------------------------------------------------
	dev -> taskController = init_TaskControl_type (taskControllerName, dev, DLGetCommonThreadPoolHndl(), ConfigureTC, UnconfigureTC, IterateTC, StartTC, 
						  ResetTC, DoneTC, StoppedTC, IterationStopTC, TaskTreeStateChange, NULL, ModuleEventHandler, ErrorTC);
	// set no iteration function timeout
	SetTaskControlIterationTimeout(dev->taskController, 0);
	
	if (!dev->taskController) goto Error;
	
	dev -> niDAQModule			= niDAQModule;
	dev -> attr					= NULL;  
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
	
	
	// assign device attributes
	dev -> attr	= *attrPtr;
	*attrPtr = NULL;
	
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

/// HIFN Lists all installed DAQmx devices.
/// HIRET List of DevAttr_type* elements containing the attributes of the found devices if successful, or 0 if function failed.
static ListType ListAllDAQmxDevices (void)
{
INIT_ERR

	int 						bufferSize  	= 0;
	ListType					devList			= 0;
	DevAttr_type* 				devAttr			= NULL;
	char* 						tmpNames		= NULL;
	char* 						tmpSubStr		= NULL;
	char* 						idxNames  		= NULL;          // used to break up the names string
	char* 						idxStr    		= NULL;          // used to break up the other strings like AI channels
	char* 						idxCopy			= NULL;			 // used to release original idx string
	char* 						devName    		= NULL;   
	int 						nDev			= 0;
	
	AIChannel_type*				newAIChan		= NULL;
	AOChannel_type*				newAOChan		= NULL;
	DILineChannel_type*			newDILineChan	= NULL;
	DIPortChannel_type*			newDIPortChan	= NULL;
	DOLineChannel_type*			newDOLineChan	= NULL;
	DOPortChannel_type*			newDOPortChan	= NULL;
	CIChannel_type*				newCIChan		= NULL;
	COChannel_type*				newCOChan		= NULL;
	
	
	
	// init
	nullChk( devList = ListCreate(sizeof(DevAttr_type*)) );
	
	// get string of device names in the computer
	//-------------------------------------------------------------------------------
	
	errChk( bufferSize = DAQmxGetSystemInfoAttribute (DAQmx_Sys_DevNames, NULL) );   
	if (!bufferSize) 
		return devList; // return empty list if no devices were found
	
	nullChk(tmpNames = malloc (bufferSize));
	errChk( DAQmxGetSystemInfoAttribute (DAQmx_Sys_DevNames, tmpNames, bufferSize) );
	
	// populate device structure with device info
	//-------------------------------------------------------------------------------
	
	idxNames 	= tmpNames;
	devName 	= substr (", ", &idxNames);
	
	while (devName) {
		
		nDev++;
		nullChk( devAttr = init_DevAttr_type() ); 
		
		//---------------------------------------------------------------------------------------------------------------------------------
		// General device attributes
		//---------------------------------------------------------------------------------------------------------------------------------
		
		// 1. Name
		devAttr->name = StrDup(devName);
		
		// 2. Type
		errChk( bufferSize = DAQmxGetDeviceAttribute(devName, DAQmx_Dev_ProductType, NULL) );
		nullChk( devAttr->type = malloc(bufferSize) );
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_ProductType, devAttr->type, bufferSize) );
		
		// 3. Product Number
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_ProductNum, &devAttr->productNum) );
		
		// 4. S/N
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_SerialNum, &devAttr->serial) ); 
		
		//---------------------------------------------------------------------------------------------------------------------------------
		// Channel properties							
		//---------------------------------------------------------------------------------------------------------------------------------
		
		// 5. AI
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AI_PhysicalChans, NULL) );
		
		if (bufferSize) {   // process info if available  
			
			nullChk( idxStr = malloc (bufferSize) );
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AI_PhysicalChans, idxStr, bufferSize) );
	   		OKfree(tmpSubStr);											
			tmpSubStr = substr (", ", &idxStr);	
			
			while (tmpSubStr) {												
				
				nullChk( newAIChan = init_AIChannel_type() );
				
				newAIChan->physChanName 			= StrDup(tmpSubStr);
				newAIChan->supportedMeasTypes 		= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_AI_SupportedMeasTypes);  
				newAIChan->Vrngs					= GetIORanges(devName, DAQmx_Dev_AI_VoltageRngs);
				newAIChan->Irngs					= GetIORanges(devName, DAQmx_Dev_AI_CurrentRngs);
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_AI_TermCfgs, &newAIChan->terminalCfg) ); 
				
				ListInsertItem (devAttr->AIchan, &newAIChan, END_OF_LIST);
				OKfree(tmpSubStr);    
				tmpSubStr = substr (", ", &idxStr); 									
			}
			
			OKfree(idxCopy);  
		}
		
		// 6. AO
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AO_PhysicalChans, NULL) );  
		
		if (bufferSize) {   //process info if available
			
			nullChk( idxStr = malloc ( bufferSize) );
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AO_PhysicalChans, idxStr, bufferSize) );
			OKfree(tmpSubStr);												
			tmpSubStr = substr (", ", &idxStr);
			
			while (tmpSubStr) {	
				
				nullChk( newAOChan = init_AOChannel_type() );
				
				newAOChan->physChanName 			= StrDup(tmpSubStr);
				newAOChan->supportedOutputTypes 	= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_AO_SupportedOutputTypes);  
				newAOChan->Vrngs					= GetIORanges(devName, DAQmx_Dev_AO_VoltageRngs);
				newAOChan->Irngs					= GetIORanges(devName, DAQmx_Dev_AO_CurrentRngs);
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_AO_TermCfgs, &newAOChan->terminalCfg) ); 
				
				ListInsertItem (devAttr->AOchan, &newAOChan, END_OF_LIST);
				OKfree(tmpSubStr);
				tmpSubStr = substr (", ", &idxStr); 									
			}
			
			OKfree(idxCopy); 
		}
		
		// 7. DI lines
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DI_Lines, NULL) );  
		
		if (bufferSize) {   //process info if available 
			
			nullChk( idxStr = malloc ( bufferSize) ); 
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DI_Lines, idxStr, bufferSize) );
			OKfree(tmpSubStr); 
			tmpSubStr = substr (", ", &idxStr);
			
			while (tmpSubStr) {
				
				nullChk( newDILineChan = init_DILineChannel_type() );	
				
				newDILineChan->physChanName 		= StrDup(tmpSubStr);
				newDILineChan->sampModes			= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_DI_SampModes);  
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DI_ChangeDetectSupported, &newDILineChan->changeDetectSupported) );
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DI_SampClkSupported, &newDILineChan->sampClkSupported) );
				
				ListInsertItem (devAttr->DIlines, &newDILineChan, END_OF_LIST);
				OKfree(tmpSubStr);
				tmpSubStr = substr (", ", &idxStr); 									
			}
			
			OKfree(idxCopy);  
		}
		
		// 8. DI ports
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DI_Ports, NULL) );
		
		if (bufferSize) {   //process info if available  
			
			nullChk( idxStr = malloc ( bufferSize) ); 
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DI_Ports, idxStr, bufferSize) );
			OKfree(tmpSubStr);													
			tmpSubStr = substr (", ", &idxStr);
			
			while (tmpSubStr) {	
				
				nullChk( newDIPortChan = init_DIPortChannel_type() );
				
				newDIPortChan->physChanName 			= StrDup(tmpSubStr);
				newDIPortChan->sampModes				= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_DI_SampModes);  
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DI_ChangeDetectSupported, &newDIPortChan->changeDetectSupported) );
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DI_SampClkSupported, &newDIPortChan->sampClkSupported) );
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DI_PortWidth, &newDIPortChan->portWidth) );
				
				ListInsertItem (devAttr->DIports, &newDIPortChan, END_OF_LIST);
				OKfree(tmpSubStr);
				tmpSubStr = substr (", ", &idxStr); 									
			}
			
			OKfree(idxCopy);  
		}
		
		// 9. DO lines
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DO_Lines, NULL) );  
		
		if (bufferSize) {   //process info if available 
			
			nullChk( idxStr = malloc ( bufferSize) ); 
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DO_Lines, idxStr, bufferSize) );
			OKfree(tmpSubStr); 														
			tmpSubStr = substr (", ", &idxStr);	
			
			while (tmpSubStr) {
				
				nullChk( newDOLineChan = init_DOLineChannel_type() );
				
				newDOLineChan->physChanName 			= StrDup(tmpSubStr);
				newDOLineChan->sampModes				= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_DO_SampModes);  
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DO_SampClkSupported, &newDOLineChan->sampClkSupported) );
				
				ListInsertItem (devAttr->DOlines, &newDOLineChan, END_OF_LIST);
				OKfree(tmpSubStr);
				tmpSubStr = substr (", ", &idxStr); 									
			} 
			
			OKfree(idxCopy);  
		}
		
		// 10. DO ports
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DO_Ports, NULL) );
		
		if (bufferSize) {   //process info if available    
			
			nullChk( idxStr = malloc ( bufferSize) ); 
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DO_Ports, idxStr, bufferSize) );
			OKfree(tmpSubStr);														
			tmpSubStr = substr (", ", &idxStr);
			
			while (tmpSubStr) {	
				
				nullChk( newDOPortChan = init_DOPortChannel_type() );
				
				newDOPortChan->physChanName 			= StrDup(tmpSubStr);
				newDOPortChan->sampModes				= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_DO_SampModes);  
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DO_SampClkSupported, &newDOPortChan->sampClkSupported) );
				errChk( DAQmxGetPhysicalChanAttribute(tmpSubStr, DAQmx_PhysicalChan_DO_PortWidth, &newDOPortChan->portWidth) );
				
				ListInsertItem (devAttr->DOports, &newDOPortChan, END_OF_LIST);
				OKfree(tmpSubStr);
				tmpSubStr = substr (", ", &idxStr); 									
			}
			
			OKfree(idxCopy);  
		}
		
		// 11. CI 
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_CI_PhysicalChans, NULL) );
		
		if (bufferSize) {   //process info if available 
			
			nullChk( idxStr = malloc ( bufferSize) ); 
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_CI_PhysicalChans, idxStr, bufferSize) );
			OKfree(tmpSubStr);														
			tmpSubStr = substr (", ", &idxStr);										
			
			while (tmpSubStr) {	
				
				nullChk( newCIChan = init_CIChannel_type() );
				
				newCIChan->physChanName 			= StrDup(tmpSubStr);
				newCIChan->supportedMeasTypes 		= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_CI_SupportedMeasTypes);  
				
				ListInsertItem (devAttr->CIchan, &newCIChan, END_OF_LIST);
				OKfree(tmpSubStr);
				tmpSubStr = substr (", ", &idxStr); 									
			}
			
			OKfree(idxCopy);  
		}
		
		// 12. CO 
		errChk( bufferSize = DAQmxGetDeviceAttribute (devName, DAQmx_Dev_CO_PhysicalChans, NULL) );
		
		if (bufferSize) {   // process info if available
			
			nullChk( idxStr = malloc ( bufferSize) );
			idxCopy = idxStr;
			errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_CO_PhysicalChans, idxStr, bufferSize) );
			OKfree(tmpSubStr); 													
			tmpSubStr = substr (", ", &idxStr);
			
			while (tmpSubStr) {												
				
				nullChk( newCOChan = init_COChannel_type() );
				
				newCOChan->physChanName 			= StrDup(tmpSubStr);
				newCOChan->supportedOutputTypes 	= GetPhysChanPropertyList(tmpSubStr, DAQmx_PhysicalChan_CO_SupportedOutputTypes);  
				
				ListInsertItem (devAttr->COchan, &newCOChan, END_OF_LIST);
				OKfree(tmpSubStr);
				tmpSubStr = substr (", ", &idxStr); 									
			}
			
			OKfree(idxCopy); 
		}
		
		//---------------------------------------------------------------------------------------------------------------------------------
		// DAQmx task properties
		//---------------------------------------------------------------------------------------------------------------------------------
		
		// 13. Single Channel AI max rate
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AI_MaxSingleChanRate, &devAttr->AISCmaxrate) ); 	// [Hz]
		
		// 14. Multiple Channel AI max rate
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AI_MaxMultiChanRate, &devAttr->AIMCmaxrate) );  	// [Hz]
		
		// 15. AI min rate 
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AI_MinRate, &devAttr->AIminrate) );  	        	// [Hz] 
		
		// Retrieving these attributes for simulated devices generates error (property not suported)
		// This is quite strange cause this misses the point of a simulated device
		
		// 16. AO max rate  
//		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AO_MaxRate, &devAttr->AOmaxrate) ); 			 	// [Hz]
		
		// 17. AO min rate  
//		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AO_MinRate, &devAttr->AOminrate) ); 			 	// [Hz]
		
		// 18. DI max rate
//		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DI_MaxRate, &devAttr->DImaxrate) ); 			 	// [Hz]
		
		// 19. DO max rate
//		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DO_MaxRate, &devAttr->DOmaxrate) );    		 		// [Hz]
		
		//---------------------------------------------------------------------------------------------------------------------------------	
		// Triggering
		//---------------------------------------------------------------------------------------------------------------------------------
		
		// Analog triggering supported ?
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AnlgTrigSupported, &devAttr->AnalogTriggering) );
		
		// Digital triggering supported ?
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DigTrigSupported, &devAttr->DigitalTriggering) );
		
		// Get types of AI trigger usage
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AI_TrigUsage, &devAttr->AITriggerUsage) );
		
		// Get types of AO trigger usage
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_AO_TrigUsage, &devAttr->AOTriggerUsage) );
		
		// Get types of DI trigger usage
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DI_TrigUsage, &devAttr->DITriggerUsage) );
		
		// Get types of DO trigger usage
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_DO_TrigUsage, &devAttr->DOTriggerUsage) );
		
		// Get types of CI trigger usage
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_CI_TrigUsage, &devAttr->CITriggerUsage) );
		
		// Get types of CO trigger usage
		errChk( DAQmxGetDeviceAttribute (devName, DAQmx_Dev_CO_TrigUsage, &devAttr->COTriggerUsage) );
		
		//---------------------------------------------------------------------------------------------------------------------------------
		// Supported IO Types
		//---------------------------------------------------------------------------------------------------------------------------------
		
		// AI
		devAttr->AISupportedMeasTypes	= GetSupportedIOTypes(devName, DAQmx_Dev_AI_SupportedMeasTypes);
		
		// AO
		devAttr->AOSupportedOutputTypes	= GetSupportedIOTypes(devName, DAQmx_Dev_AO_SupportedOutputTypes);
		
		// CI
		devAttr->CISupportedMeasTypes	= GetSupportedIOTypes(devName, DAQmx_Dev_CI_SupportedMeasTypes);
		
		// CO
		devAttr->COSupportedOutputTypes	= GetSupportedIOTypes(devName, DAQmx_Dev_CO_SupportedOutputTypes);
		
		
		//--------------------------------------------------------------------------------------------------------------------------------- 
		// end of device attributes
		//---------------------------------------------------------------------------------------------------------------------------------
		
		// add device to list
		ListInsertItem(devList, &devAttr, END_OF_LIST);
		
		// get the next device name in the list
		OKfree(devName);
		devName = substr (", ", &idxNames); 
		
	}
	
	// cleanup
	OKfree(idxNames);
	OKfree(idxStr);
	OKfree(tmpNames);
	
	return devList;
	
Error:
	
	OKfree(tmpNames);
	OKfree(tmpSubStr);
	OKfree(idxNames);
	OKfree(idxStr);
	OKfree(idxCopy);
	OKfree(devName);
	discard_DevAttr_type(&devAttr);
	
	discard_AIChannel_type(&newAIChan);
	discard_AOChannel_type(&newAOChan);
	discard_DILineChannel_type(&newDILineChan);
	discard_DIPortChannel_type(&newDIPortChan);
	discard_DOLineChannel_type(&newDOLineChan);
	discard_DOPortChannel_type(&newDOPortChan);
	discard_CIChannel_type(&newCIChan);
	discard_COChannel_type(&newCOChan);
	
	DiscardDeviceList(&devList);
	
	return 0;
	
}

/// HIFN Given a list of devices as Dev_Attr_type* elements, the function populates a table with device properties
static void UpdateDeviceList (ListType devList, int panHndl, int tableCtrl)
{
// Macro to write DAQ Device data to table
#define DAQTable(cell, info) SetTableCellVal (panHndl, tableCtrl , MakePoint (cell, i), info)     
	
	//int							error				= 0;
	size_t						nDevs        		= ListNumItems(devList);	// # devices
	DevAttr_type* 				devAttr				= NULL; 
	int 						nCols	     		= 0;
	
	
	// get number of table columns
	GetNumTableColumns (panHndl, tableCtrl, &nCols);
	
	// empty table contents
	DeleteTableRows (panHndl, tableCtrl, 1, -1);
	
	// insert table rows, set cell attributes, highlight first row and select first device
	if (nDevs) {
		SetTableColumnAttribute (panHndl, tableCtrl, -1, ATTR_CELL_JUSTIFY, VAL_CENTER_CENTER_JUSTIFIED);
    	InsertTableRows (panHndl, tableCtrl, -1, nDevs, VAL_USE_MASTER_CELL_TYPE);
		SetTableCellRangeAttribute (panHndl, tableCtrl, MakeRect(1, 1, nDevs, nCols), ATTR_CELL_MODE , VAL_INDICATOR);
		SetTableCellRangeAttribute (panHndl, tableCtrl, MakeRect(1, 1, 1, nCols),ATTR_TEXT_BGCOLOR, VAL_YELLOW);
		SetCtrlAttribute (panHndl, tableCtrl, ATTR_LABEL_TEXT, "Devices found");  
		currDev = 1; // global var
	} else {
		SetCtrlAttribute (panHndl, DevListPan_DAQTable, ATTR_LABEL_TEXT,"No devices found"); 
		return;
	}
	
	// fill table with device info
	for (size_t i = 1; i <= nDevs; i++) {
		
		devAttr = *(DevAttr_type**)ListGetPtrToItem(devList, i);
		
		// 1. Name
		DAQTable(1, devAttr->name);
		// 2. Type
		DAQTable(2, devAttr->type); 
		// 3. Product Number
		DAQTable(3, devAttr->productNum); 
		// 4. S/N
		DAQTable(4, devAttr->serial);
		// 5. # AI chan 
		DAQTable(5, ListNumItems(devAttr->AIchan));
		// 6. # AO chan
		DAQTable(6, ListNumItems(devAttr->AOchan));
		// 7. # DI lines
		DAQTable(7, ListNumItems(devAttr->DIlines));
		// 8. # DI ports  
		DAQTable(8, ListNumItems(devAttr->DIports));
		// 9. # DO lines
		DAQTable(9, ListNumItems(devAttr->DOlines));
		// 10. # DO ports  
		DAQTable(10, ListNumItems(devAttr->DOports));
		// 11. # CI 
		DAQTable(11, ListNumItems(devAttr->CIchan));
		// 12. # CO ports  
		DAQTable(12, ListNumItems(devAttr->COchan));
		// 13. Single Channel AI max rate  
		DAQTable(13, devAttr->AISCmaxrate/1000); // display in [kHz]
		// 14. Multiple Channel AI max rate 
		DAQTable(14, devAttr->AIMCmaxrate/1000); // display in [kHz] 
		// 15. AI min rate
		DAQTable(15, devAttr->AIminrate/1000);   // display in [kHz] 
		// 16. AO max rate
		DAQTable(16, devAttr->AOmaxrate/1000);   // display in [kHz]
		// 17. AO min rate  
		DAQTable(17, devAttr->AOminrate/1000);   // display in [kHz] 
		// 18. DI max rate
		DAQTable(18, devAttr->DImaxrate/1000);   // display in [kHz] 
		// 19. DO max rate  
		DAQTable(19, devAttr->DOmaxrate/1000);   // display in [kHz]   
	}
	
}

static void DiscardDeviceList (ListType* devList)
{
    DevAttr_type** 	devAttrPtr	= NULL;
	
	if (!*devList) return;
	
	size_t			nDevs		= ListNumItems(*devList);
	
	for (size_t i = 1; i <= nDevs; i++) {
		devAttrPtr = ListGetPtrToItem(*devList, i);
		discard_DevAttr_type(devAttrPtr);
	}
	
	OKfreeList(*devList);
}

//------------------------------------------------------------------------------
// DevAttr_type Device attributes data structure
//------------------------------------------------------------------------------

static DevAttr_type* init_DevAttr_type(void)
{
INIT_ERR
	
	DevAttr_type* 	attr 			= malloc (sizeof(DevAttr_type));
	if (!attr) return NULL;
	
	attr -> name         			= NULL; // dynamically allocated string
	attr -> type         			= NULL; // dynamically allocated string
	attr -> productNum				= 0;	// unique product number
	attr -> serial       			= 0;   	// device serial number
	attr -> AISCmaxrate  			= 0;   	// Single channel AI max rate
	attr -> AIMCmaxrate  			= 0;   	// Multiple channel AI max rate
	attr -> AIminrate    			= 0;
	attr -> AOmaxrate    			= 0;
	attr -> AOminrate    			= 0;
	attr -> DImaxrate    			= 0;
	attr -> DOmaxrate				= 0;
	
	// init lists
	attr -> AIchan					= 0;
	attr -> AOchan					= 0;
	attr -> DIlines					= 0;
	attr -> DIports					= 0;
	attr -> DOlines					= 0;
	attr -> DOports					= 0;
	attr -> CIchan					= 0;
	attr -> COchan					= 0;
	
	// create list of channels
	nullChk(attr -> AIchan   		= ListCreate (sizeof(AIChannel_type*)));
	nullChk(attr -> AOchan	  		= ListCreate (sizeof(AOChannel_type*)));
	nullChk(attr -> DIlines  		= ListCreate (sizeof(DILineChannel_type*)));
	nullChk(attr -> DIports  		= ListCreate (sizeof(DIPortChannel_type*)));
	nullChk(attr -> DOlines  		= ListCreate (sizeof(DOLineChannel_type*)));
	nullChk(attr -> DOports  		= ListCreate (sizeof(DOPortChannel_type*)));
	nullChk(attr -> CIchan   		= ListCreate (sizeof(CIChannel_type*)));
	nullChk(attr -> COchan   		= ListCreate (sizeof(COChannel_type*)));
	
	// triggering
	attr -> AnalogTriggering		= 0;
	attr -> DigitalTriggering		= 0;
	
	attr -> AITriggerUsage			= 0;
	attr -> AOTriggerUsage			= 0;
	attr -> DITriggerUsage			= 0;
	attr -> DOTriggerUsage			= 0;
	attr -> CITriggerUsage			= 0;
	attr -> COTriggerUsage			= 0;
	
	// supported IO types list init
	attr -> AISupportedMeasTypes	= 0;
	attr -> AOSupportedOutputTypes	= 0;
	attr -> CISupportedMeasTypes	= 0;
	attr -> COSupportedOutputTypes	= 0;
	
	return attr;
	
Error:
	
	discard_DevAttr_type(&attr);
	
	return NULL;
}



static void discard_DevAttr_type(DevAttr_type** attrPtr)
{
	DevAttr_type*	attr = *attrPtr;
	
	if (!*attrPtr) return;
	
	OKfree(attr->name);
	OKfree(attr->type);
	
	// discard channel lists and free pointers within lists.
	if (attr->AIchan) { 	
		ListApplyToEachEx (attr->AIchan, 1, DisposeAIChannelList, NULL); 
		OKfreeList(attr->AIchan);
	}						   
	
	if (attr->AOchan) {
		ListApplyToEachEx (attr->AOchan, 1, DisposeAOChannelList, NULL);
		OKfreeList(attr->AOchan);
	}
	
	if (attr->DIlines) {
		ListApplyToEachEx (attr->DIlines, 1, DisposeDILineChannelList, NULL);
		OKfreeList(attr->DIlines);
	}
	
	if (attr->DIports) {
		ListApplyToEachEx (attr->DIports, 1, DisposeDIPortChannelList, NULL); 
		OKfreeList(attr->DIports);
	}
	
	if (attr->DOlines) {
		ListApplyToEachEx (attr->DOlines, 1, DisposeDOLineChannelList, NULL);
		OKfreeList(attr->DOlines);
	}
	
	if (attr->DOports) {
		ListApplyToEachEx (attr->DOports, 1, DisposeDOPortChannelList, NULL);
		OKfreeList(attr->DOports); 
	}
	
	if (attr->CIchan) {
		ListApplyToEachEx (attr->CIchan, 1, DisposeCIChannelList, NULL);
		OKfreeList(attr->CIchan); 
	}
	
	if (attr->COchan) {
		ListApplyToEachEx (attr->COchan, 1, DisposeCOChannelList, NULL);
		OKfreeList(attr->COchan); 
	}
	
	// discard supported IO type lists
	OKfreeList(attr->AISupportedMeasTypes);
	OKfreeList(attr->AOSupportedOutputTypes);
	OKfreeList(attr->CISupportedMeasTypes);
	OKfreeList(attr->COSupportedOutputTypes);
	
	OKfree(*attrPtr);
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
	AIChannel_type* 	chan = malloc (sizeof(AIChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 			= NULL;
	chan->supportedMeasTypes   	= 0;
	chan->Vrngs					= NULL;
	chan->Irngs					= NULL;
	chan->terminalCfg			= 0;
	chan->inUse					= FALSE;
	
	return chan;
}

static void discard_AIChannel_type (AIChannel_type** chanPtr)
{
	AIChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->supportedMeasTypes);
	discard_IORange_type(&chan->Vrngs);
	discard_IORange_type(&chan->Irngs);
	
	OKfree(*chanPtr);
}

int CVICALLBACK DisposeAIChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	AIChannel_type** chanPtr = ptrToItem;
	
	discard_AIChannel_type(chanPtr);
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
	AIChannel_type** 	chPtr;
	size_t				nChans		= ListNumItems(dev->attr->AIchan); 	
	for (size_t i = 1; i <= nChans; i++) { 
		chPtr = ListGetPtrToItem(dev->attr->AIchan, i);
		if (!strcmp((*chPtr)->physChanName, physChanName))
			return *chPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// AOChannel_type
//------------------------------------------------------------------------------
static AOChannel_type* init_AOChannel_type (void)
{
	AOChannel_type* chan = malloc (sizeof(AOChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 				= NULL;
	chan->supportedOutputTypes		= 0;
	chan->Vrngs						= NULL;
	chan->Irngs						= NULL;
	chan->terminalCfg				= 0;
	chan->inUse						= FALSE;
	
	return chan;
}

static void discard_AOChannel_type (AOChannel_type** chanPtr)
{
	AOChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->supportedOutputTypes);
	discard_IORange_type(&chan->Vrngs);
	discard_IORange_type(&chan->Irngs);
	
	OKfree(*chanPtr);
}

int CVICALLBACK DisposeAOChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	AOChannel_type** chanPtr = ptrToItem;
	
	discard_AOChannel_type(chanPtr);
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
	AOChannel_type** 	chPtr;
	size_t				nChans		= ListNumItems(dev->attr->AOchan); 	
	for (size_t i = 1; i <= nChans; i++) {
		chPtr = ListGetPtrToItem(dev->attr->AOchan, i); 
		if (!strcmp((*chPtr)->physChanName, physChanName))
			return *chPtr;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// DILineChannel_type
//------------------------------------------------------------------------------
static DILineChannel_type* init_DILineChannel_type (void)
{
	DILineChannel_type* 	chan = malloc (sizeof(DILineChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 				= NULL;
	chan->sampModes					= 0;
	chan->changeDetectSupported		= 0;
	chan->sampClkSupported			= 0;
	chan->inUse						= FALSE;
	
	return chan;
}

static void discard_DILineChannel_type (DILineChannel_type** chanPtr)
{
	DILineChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->sampModes);
	
	OKfree(*chanPtr);
}

int CVICALLBACK DisposeDILineChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DILineChannel_type** chanPtr = ptrToItem;
	
	discard_DILineChannel_type(chanPtr);
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
	DIPortChannel_type* 	chan = malloc (sizeof(DIPortChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 				= NULL;
	chan->sampModes					= 0;
	chan->changeDetectSupported		= 0;
	chan->sampClkSupported			= 0;
	chan->portWidth					= 0;
	chan->inUse						= FALSE;
	
	return chan;
}

static void discard_DIPortChannel_type (DIPortChannel_type** chanPtr)
{
	DIPortChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->sampModes);
	
	OKfree(*chanPtr);
}

int CVICALLBACK DisposeDIPortChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DIPortChannel_type** chanPtr = ptrToItem;
	
	discard_DIPortChannel_type(chanPtr);
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
	DOLineChannel_type* 	chan = malloc (sizeof(DOLineChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 			= NULL;
	chan->sampModes				= 0;
	chan->sampClkSupported		= 0;
	chan->inUse					= FALSE;
	
	return chan;
}

static void discard_DOLineChannel_type (DOLineChannel_type** chanPtr)
{
	DOLineChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->sampModes);
	
	OKfree(*chanPtr);
}

int CVICALLBACK DisposeDOLineChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DOLineChannel_type** chanPtr = ptrToItem;
	
	discard_DOLineChannel_type(chanPtr);
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
	DOPortChannel_type* 	chan = malloc (sizeof(DOPortChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 			= NULL;
	chan->sampModes				= 0;
	chan->sampClkSupported		= 0;
	chan->portWidth				= 0;
	chan->inUse					= FALSE;
	
	return chan;
}

static void discard_DOPortChannel_type (DOPortChannel_type** chanPtr)
{
	DOPortChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->sampModes);
	
	OKfree(*chanPtr);
}

int CVICALLBACK DisposeDOPortChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DOPortChannel_type** chanPtr = ptrToItem;
	
	discard_DOPortChannel_type(chanPtr);
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
	CIChannel_type* 	chan = malloc (sizeof(CIChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 			= NULL;
	chan->supportedMeasTypes   	= 0;
	chan->inUse					= FALSE;
	
	return chan;
}

static void discard_CIChannel_type (CIChannel_type** chanPtr)
{
	CIChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->supportedMeasTypes);
	
	OKfree(*chanPtr);
}

int CVICALLBACK DisposeCIChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	CIChannel_type** chanPtr = ptrToItem;
	
	discard_CIChannel_type(chanPtr);
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
	COChannel_type* 	chan = malloc (sizeof(COChannel_type));
	if (!chan) return NULL;
	
	chan->physChanName 			= NULL;
	chan->supportedOutputTypes  = 0;
	chan->inUse					= FALSE;
	
	return chan;
}

static void discard_COChannel_type (COChannel_type** chanPtr)
{
	COChannel_type*		chan = *chanPtr;
	
	if (!chan) return;
	
	OKfree(chan->physChanName);
	OKfreeList(chan->supportedOutputTypes);
	
	OKfree(*chanPtr);
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

static void  init_ChanSet_type (Dev_type* dev, ChanSet_type* chanSet, char physChanName[], Channel_type chanType, DiscardFptr_type discardFptr)
{
	chanSet->name							= StrDup(physChanName);
	chanSet->chanType						= chanType;
	chanSet->chanPanHndl					= 0;
	chanSet->sinkVChan						= NULL;
	chanSet->device							= dev;
	chanSet->srcVChan						= NULL;
	chanSet->onDemand						= FALSE;				// hw-timing by default
	chanSet->integrateFlag					= FALSE;
	chanSet->dataTypeConversion.dataType	= Convert_To_Double;   // no conversion needed, gai, offset, min and max are ignored.
	chanSet->dataTypeConversion.gain		= 1;
	chanSet->dataTypeConversion.offset		= 0;
	chanSet->dataTypeConversion.min			= 0;
	chanSet->dataTypeConversion.max			= 0;
	chanSet->discardFptr					= discardFptr;
}

static void	discard_ChanSet_type (ChanSet_type** chanSetPtr)
{
	ChanSet_type*	chanSet = *chanSetPtr;
	
	if (!chanSet) return;
	
	OKfree(chanSet->name);
	
	// VChans
	discard_VChan_type((VChan_type**)&chanSet->srcVChan);
	discard_VChan_type((VChan_type**)&chanSet->sinkVChan);
	
	OKfree(*chanSetPtr);
}

//------------------------------------------------------------------------------
// ChanSet_AI_Voltage_type
//------------------------------------------------------------------------------

static ChanSet_AI_Voltage_type* init_ChanSet_AI_Voltage_type (Dev_type* dev, char physChanName[], AIChannel_type* chanAttr, BOOL integrateFlag, 
															  DataTypeConversion_type dataTypeConversion, uInt32 VRangeIdx, Terminal_type terminalType)
{
	ChanSet_AI_Voltage_type* chan = malloc (sizeof(ChanSet_AI_Voltage_type));
	if (!chan) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &chan->baseClass, physChanName, Chan_AI_Voltage, (DiscardFptr_type)discard_ChanSet_AI_Voltage_type);
	chan->baseClass.dataTypeConversion	= dataTypeConversion;
	chan->baseClass.integrateFlag		= integrateFlag;
	
	// init child
	chan->chanAttr							= chanAttr;
	chan->chanAttr->Vrngs->selectedRange	= VRangeIdx;
	chan->VMax								= chan->chanAttr->Vrngs->high[VRangeIdx];
	chan->VMin								= chan->chanAttr->Vrngs->low[VRangeIdx];
	chan->terminal							= terminalType;
	
	return chan;
}

static void discard_ChanSet_AI_Voltage_type (ChanSet_AI_Voltage_type** chanSetPtr)
{
	if (!*chanSetPtr) return;
	
	// discard base class
	discard_ChanSet_type((ChanSet_type**)chanSetPtr);
}

static int AddToUI_Chan_AI_Voltage (ChanSet_AI_Voltage_type* chanSet)
{
	int					error			= 0;
	int 				chanSetPanHndl  = 0;
	int 				chanPanHndl		= 0;
	int    				newTabIdx		= 0;  
	char* 				shortChanName	= NULL;
	char*				VChanName		= NULL;
	Dev_type*			dev				= chanSet->baseClass.device;
	
	//-------------------------------
	// Prepare channel settings UI
	//-------------------------------
	
	GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_ChanTabIdx, &chanPanHndl);
	// insert new channel settings tab
	chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AIVoltage);  
	newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
	
	// change tab title
	shortChanName = strstr(chanSet->baseClass.name, "/") + 1;  
	SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
	OKfreePanHndl(chanSetPanHndl);
	
	GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &chanSet->baseClass.chanPanHndl);
	// add callbackdata to the channel panel
	SetPanelAttribute(chanSet->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, chanSet);
	// add callback data to the controls in the panel
	SetCtrlsInPanCBInfo(chanSet, ChanSetAIVoltageUI_CB, chanSet->baseClass.chanPanHndl);
							
	//--------------
	// Terminal
	//--------------
	
	// select default terminal configuration from available choices in the following order
	if (chanSet->terminal == Terminal_None)
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_RSE) 
		chanSet->terminal = Terminal_RSE;
	else
		if (chanSet->chanAttr->terminalCfg | Terminal_Bit_NRSE) 
			chanSet->terminal = Terminal_NRSE;
		else
			if (chanSet->chanAttr->terminalCfg | Terminal_Bit_Diff) 
				chanSet->terminal = Terminal_Diff;
			else
				if (chanSet->chanAttr->terminalCfg | Terminal_Bit_PseudoDiff) 
					chanSet->terminal = Terminal_PseudoDiff; // if none of the above then Terminal_None is used which should not happen
									
	// populate terminal ring control with available terminal configurations
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_RSE) InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, -1, "RSE", Terminal_RSE);  
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_NRSE) InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, -1, "NRSE", Terminal_NRSE);  
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_Diff) InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, -1, "Diff", Terminal_Diff);  
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_PseudoDiff) InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, -1, "PseudoDiff", Terminal_PseudoDiff);
	
	// select terminal to match previously assigned value to chanSet->terminal
	int terminalIdx = 0;
	GetIndexFromValue(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, &terminalIdx, chanSet->terminal);
	if (terminalIdx >= 0) {
		SetCtrlIndex(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, terminalIdx);
		SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, ATTR_DIMMED, FALSE);
	} else
		SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, ATTR_DIMMED, TRUE);
							
	//--------------
	// AI ranges
	//--------------
	
	// insert AI ranges
	char label[50] = "";
	for (int i = 0; i < chanSet->chanAttr->Vrngs->Nrngs ; i++) {
		sprintf(label, "%.2f, %.2f", chanSet->chanAttr->Vrngs->low[i], chanSet->chanAttr->Vrngs->high[i]);
		InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_Range, -1, label, i);
	}
	
	// update to current range selection
	SetCtrlIndex(chanSet->baseClass.chanPanHndl, AIVoltage_Range, chanSet->chanAttr->Vrngs->selectedRange);
	
	//------------------
	// Integration mode
	//------------------
							
	SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_Integrate, chanSet->baseClass.integrateFlag);
	
	//---------------------------
	// Adjust data type scaling
	//---------------------------
	
	// offset
	SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_Offset, chanSet->baseClass.dataTypeConversion.offset); 
	
	// gain
	SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_Gain, chanSet->baseClass.dataTypeConversion.gain); 
	
	// max value
	SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, chanSet->baseClass.dataTypeConversion.max); 
	SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, ATTR_MAX_VALUE, chanSet->VMax);
	SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, ATTR_MIN_VALUE, chanSet->VMin);
							
	// min value
	SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, chanSet->baseClass.dataTypeConversion.min); 
	SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, ATTR_MAX_VALUE, chanSet->VMax);
	SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, ATTR_MIN_VALUE, chanSet->VMin);
	
	switch(chanSet->baseClass.dataTypeConversion.dataType) {
			
		case Convert_To_Double:
		case Convert_To_Float:
			
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, ATTR_VISIBLE, FALSE); 
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, ATTR_VISIBLE, FALSE);
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Gain, ATTR_VISIBLE, TRUE);
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Offset, ATTR_VISIBLE, TRUE);
			break;
			
		case Convert_To_UInt:
		case Convert_To_UShort:
		case Convert_To_UChar:
			
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, ATTR_VISIBLE, TRUE); 
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, ATTR_VISIBLE, TRUE);
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Gain, ATTR_VISIBLE, FALSE);
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Offset, ATTR_VISIBLE, FALSE);
			break;
	}
							
	//---------------------
	// Data type conversion
	//---------------------
	
	InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_AIDataType, Convert_To_Double, "double", Convert_To_Double);
	InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_AIDataType, Convert_To_Float, "float", Convert_To_Float);
	InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_AIDataType, Convert_To_UInt, "uInt", Convert_To_UInt);
	InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_AIDataType, Convert_To_UShort, "uShort", Convert_To_UShort);
	InsertListItem(chanSet->baseClass.chanPanHndl, AIVoltage_AIDataType, Convert_To_UChar, "uChar", Convert_To_UChar);
	SetCtrlIndex(chanSet->baseClass.chanPanHndl, AIVoltage_AIDataType, chanSet->baseClass.dataTypeConversion.dataType);
	
	//--------------------------
	// Create and register VChan
	//--------------------------
	
	// name of VChan is unique because because task controller names are unique and physical channel names are unique
	VChanName = GetTaskControlName(dev->taskController);
	AppendString(&VChanName, ": ", -1);
	AppendString(&VChanName, chanSet->baseClass.name, -1);
	
	
	switch (chanSet->baseClass.dataTypeConversion.dataType) {
		
		case Convert_To_Double:
			
			chanSet->baseClass.srcVChan = init_SourceVChan_type(VChanName, DL_Waveform_Double, chanSet, AIDataVChan_StateChange); 
			break;
			
		case Convert_To_Float:
			
			chanSet->baseClass.srcVChan = init_SourceVChan_type(VChanName, DL_Waveform_Float, chanSet, AIDataVChan_StateChange); 
			break;
			
		case Convert_To_UInt:
			
			chanSet->baseClass.srcVChan = init_SourceVChan_type(VChanName, DL_Waveform_UInt, chanSet, AIDataVChan_StateChange); 
			break;
			
		case Convert_To_UShort:
			
			chanSet->baseClass.srcVChan = init_SourceVChan_type(VChanName, DL_Waveform_UShort, chanSet, AIDataVChan_StateChange);
			break;
			
		case Convert_To_UChar:
			
			chanSet->baseClass.srcVChan = init_SourceVChan_type(VChanName, DL_Waveform_UChar, chanSet, AIDataVChan_StateChange);
			break;
	}
	
	
	DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)chanSet->baseClass.srcVChan);
	SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_VChanName, VChanName);
	OKfree(VChanName);
							
	return 0;
}

// AI Voltage channel UI callback for setting channel properties
static int CVICALLBACK ChanSetAIVoltageUI_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ChanSet_AI_Voltage_type*	chanSet		= (ChanSet_AI_Voltage_type*) callbackData;
	
	

	if (event != EVENT_COMMIT) return 0;	// filter only commit events
	
	switch(control) {
			
		case AIVoltage_OnDemand:
			
			GetCtrlVal(panel, control, &chanSet->baseClass.onDemand);
			
			// update device settings
			errChk( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AIVoltage_Range:
			
			GetCtrlVal(panel, control, &chanSet->chanAttr->Vrngs->selectedRange);
			
			// read in ranges
			chanSet->VMax = chanSet->chanAttr->Vrngs->high[chanSet->chanAttr->Vrngs->selectedRange];
			chanSet->VMin = chanSet->chanAttr->Vrngs->low[chanSet->chanAttr->Vrngs->selectedRange];
					
			//---------------------------
			// adjust data type scaling
			//---------------------------
			// max value
			chanSet->baseClass.dataTypeConversion.max = chanSet->VMax; 
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, ATTR_MAX_VALUE, chanSet->VMax);
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, ATTR_MIN_VALUE, chanSet->VMin);
			SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMax, chanSet->baseClass.dataTypeConversion.max); 		
				
			// min value
			chanSet->baseClass.dataTypeConversion.min = chanSet->VMin;
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, ATTR_MAX_VALUE, chanSet->VMax);
			SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, ATTR_MIN_VALUE, chanSet->VMin);
			SetCtrlVal(chanSet->baseClass.chanPanHndl, AIVoltage_ScaleMin, chanSet->baseClass.dataTypeConversion.min); 
			
			// update device settings
			errChk ( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AIVoltage_AIDataType:
			
			int AIDataTypeIdx;
			
			GetCtrlIndex(panel, control, &AIDataTypeIdx);
			chanSet->baseClass.dataTypeConversion.dataType = (DataTypeConversions) AIDataTypeIdx;
			
			BOOL showScaleControls = FALSE;
			switch (AIDataTypeIdx) {
					
				case Convert_To_Double:
					
					SetSourceVChanDataType(chanSet->baseClass.srcVChan, DL_Waveform_Double);
					ResetAIDataTypeGainOffset(chanSet);
					break;
					
				case Convert_To_Float:
					
					SetSourceVChanDataType(chanSet->baseClass.srcVChan, DL_Waveform_Float);
					ResetAIDataTypeGainOffset(chanSet);
					break;
					
				case Convert_To_UInt:
					
					SetSourceVChanDataType(chanSet->baseClass.srcVChan, DL_Waveform_UInt);
					showScaleControls = TRUE;
					break;
					
				case Convert_To_UShort:
					
					SetSourceVChanDataType(chanSet->baseClass.srcVChan, DL_Waveform_UShort);
					showScaleControls = TRUE;
					break;
					
				case Convert_To_UChar:
					
					SetSourceVChanDataType(chanSet->baseClass.srcVChan, DL_Waveform_UChar);
					showScaleControls = TRUE;
					break;
			}
			
			// dim/undim scale, gain and offset controls
			SetCtrlAttribute(panel, AIVoltage_ScaleMin, ATTR_VISIBLE, showScaleControls);
			SetCtrlAttribute(panel, AIVoltage_ScaleMax, ATTR_VISIBLE, showScaleControls);
			SetCtrlAttribute(panel, AIVoltage_Gain, ATTR_VISIBLE, !showScaleControls);
			SetCtrlAttribute(panel, AIVoltage_Offset, ATTR_VISIBLE, !showScaleControls);
			
			DLUpdateSwitchboard();
			
			errChk( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AIVoltage_Offset:
			
			GetCtrlVal(panel, control, &chanSet->baseClass.dataTypeConversion.offset);
			errChk( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AIVoltage_Gain:
			
			GetCtrlVal(panel, control, &chanSet->baseClass.dataTypeConversion.gain);
			ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg);
			break;
			
		case AIVoltage_Integrate:
			
			GetCtrlVal(panel, control, &chanSet->baseClass.integrateFlag);
			errChk( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AIVoltage_ScaleMin:
			
			GetCtrlVal(panel, control, &chanSet->baseClass.dataTypeConversion.min);
			errChk( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AIVoltage_ScaleMax:
			
			GetCtrlVal(panel, control, &chanSet->baseClass.dataTypeConversion.max);
			errChk( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
				
		case AIVoltage_Terminal:
			
			int terminalType;
			
			GetCtrlVal(panel, control, &terminalType);
			chanSet->terminal = (Terminal_type) terminalType;
			
			// update device settings
			errChk ( ConfigDAQmxAITask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AIVoltage_VChanName:
			
			char*	newName 	= NULL;
			
			newName = GetStringFromControl (panel, control);
			
			DLRenameVChan((VChan_type*)chanSet->baseClass.srcVChan, newName);
			OKfree(newName);
			break;
	}
	
	return 0;
	
Error:
	
PRINT_ERR
	
	return 0;
}

//------------------------------------------------------------------------------
// ChanSet_AO_Voltage_type
//------------------------------------------------------------------------------

static ChanSet_AO_Voltage_type* init_ChanSet_AO_Voltage_type (Dev_type* dev, char physChanName[], AOChannel_type* chanAttr, uInt32 VRangeIdx, Terminal_type terminalType)
{
	ChanSet_AO_Voltage_type* chan = malloc (sizeof(ChanSet_AO_Voltage_type));
	if (!chan) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &chan->baseClass, physChanName, Chan_AO_Voltage, (DiscardFptr_type)discard_ChanSet_AO_Voltage_type);
	
	// init child
	chan->chanAttr							= chanAttr;
	chan->chanAttr->Vrngs->selectedRange	= VRangeIdx;
	chan->VMax								= chan->chanAttr->Vrngs->high[VRangeIdx];
	chan->VMin								= chan->chanAttr->Vrngs->low[VRangeIdx];
	chan->terminal							= terminalType;
	
	return chan;
}

static void discard_ChanSet_AO_Voltage_type (ChanSet_AO_Voltage_type** chanSetPtr)
{
	if (!*chanSetPtr) return;
	
	// discard base class
	discard_ChanSet_type((ChanSet_type**)chanSetPtr);
}

static int AddToUI_Chan_AO_Voltage (ChanSet_AO_Voltage_type* chanSet)
{
INIT_ERR	
	
	int 				chanSetPanHndl  = 0;
	int 				chanPanHndl		= 0; 
	int    				newTabIdx		= 0;  
	char* 				shortChanName	= NULL;
	char*				VChanName		= NULL;
	Dev_type*			dev				= chanSet->baseClass.device;
	
	//-------------------------------
	// Prepare channel settings UI
	//-------------------------------
	
	GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_ChanTabIdx, &chanPanHndl);
	// insert new channel settings tab
	chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, AOVoltage);  
	newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
	// change tab title
	shortChanName = strstr(chanSet->baseClass.name, "/") + 1;  
	SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
	OKfreePanHndl(chanSetPanHndl);
	GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &chanSet->baseClass.chanPanHndl);
	// add callbackdata to the channel panel
	SetPanelAttribute(chanSet->baseClass.chanPanHndl, ATTR_CALLBACK_DATA, chanSet);
	// add callback data to the controls in the panel
	SetCtrlsInPanCBInfo(chanSet, ChanSetAOVoltageUI_CB, chanSet->baseClass.chanPanHndl);
							
	//--------------
	// Terminal
	//--------------
	
	// select default terminal configuration from available choices in the following order
	if (chanSet->terminal == Terminal_None)
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_RSE) 
		chanSet->terminal = Terminal_RSE;
	else
		if (chanSet->chanAttr->terminalCfg | Terminal_Bit_NRSE)
			chanSet->terminal = Terminal_NRSE;
		else
			if (chanSet->chanAttr->terminalCfg | Terminal_Bit_Diff) 
				chanSet->terminal = Terminal_Diff;
			else
				if (chanSet->chanAttr->terminalCfg | Terminal_Bit_PseudoDiff) 
					chanSet->terminal = Terminal_PseudoDiff; // if none of the above then Terminal_None is used which should not happen
											
	// populate terminal ring control with available terminal configurations
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_RSE) InsertListItem(chanSet->baseClass.chanPanHndl, AOVoltage_Terminal, -1, "RSE", Terminal_RSE);  
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_NRSE) InsertListItem(chanSet->baseClass.chanPanHndl, AOVoltage_Terminal, -1, "NRSE", Terminal_NRSE);  
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_Diff) InsertListItem(chanSet->baseClass.chanPanHndl, AOVoltage_Terminal, -1, "Diff", Terminal_Diff);  
	if (chanSet->chanAttr->terminalCfg | Terminal_Bit_PseudoDiff) InsertListItem(chanSet->baseClass.chanPanHndl, AOVoltage_Terminal, -1, "PseudoDiff", Terminal_PseudoDiff);
	
	// select terminal to match previously assigned value to chanSet->terminal
	int terminalIdx = 0;
	GetIndexFromValue(chanSet->baseClass.chanPanHndl, AOVoltage_Terminal, &terminalIdx, chanSet->terminal); 
	if (terminalIdx >= 0) {
		SetCtrlIndex(chanSet->baseClass.chanPanHndl, AOVoltage_Terminal, terminalIdx);
		SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, ATTR_DIMMED, FALSE);
	} else
		SetCtrlAttribute(chanSet->baseClass.chanPanHndl, AIVoltage_Terminal, ATTR_DIMMED, TRUE);
							
	//--------------
	// AO ranges
	//--------------
	
	// insert AO ranges and make largest range default
	char label[50] = "";
	for (int i = 0; i < chanSet->chanAttr->Vrngs->Nrngs ; i++) {
		sprintf(label, "%.2f, %.2f", chanSet->chanAttr->Vrngs->low[i], chanSet->chanAttr->Vrngs->high[i]);
		InsertListItem(chanSet->baseClass.chanPanHndl, AOVoltage_Range, -1, label, i);
	}
							
	// update to current range selection
	SetCtrlIndex(chanSet->baseClass.chanPanHndl, AOVoltage_Range, chanSet->chanAttr->Vrngs->selectedRange);
							
	//-------------------------------------------------------------
	// Create and register VChan with Task Controller and framework
	//-------------------------------------------------------------
	
	// name of VChan is unique because because task controller names are unique and physical channel names are unique
	VChanName = GetTaskControlName(dev->taskController);
	AppendString(&VChanName, ": ", -1);
	AppendString(&VChanName, chanSet->baseClass.name, -1);
	
	DLDataTypes allowedPacketTypes[] = {DL_Waveform_Double, DL_RepeatedWaveform_Double, DL_Waveform_Float, DL_RepeatedWaveform_Float, DL_Float, DL_Double};
							
	chanSet->baseClass.sinkVChan = init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), chanSet, VChan_Data_Receive_Timeout, AODataVChan_StateChange);  
	DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)chanSet->baseClass.sinkVChan);
	SetCtrlVal(chanSet->baseClass.chanPanHndl, AOVoltage_VChanName, VChanName);
	errChk( AddSinkVChan(dev->taskController, chanSet->baseClass.sinkVChan, AO_DataReceivedTC, NULL) ); 
	OKfree(VChanName);
							
	return 0;
	
Error:
	
	return errorInfo.error;
	
}

static int CVICALLBACK ChanSetAOVoltageUI_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR	
	
	ChanSet_AO_Voltage_type*	chanSet				= (ChanSet_AO_Voltage_type*) callbackData;
	
	if (event != EVENT_COMMIT) return 0;   // filter only commit events
	
	switch(control) {
			
		case AOVoltage_OnDemand:
			
			GetCtrlVal(panel, control, &chanSet->baseClass.onDemand);
			
			if (chanSet->baseClass.onDemand) {
				DLDataTypes		allowedTypes[] = {DL_Waveform_Double, DL_RepeatedWaveform_Double, DL_Waveform_Float, DL_RepeatedWaveform_Float, DL_Float, DL_Double};
				SetSinkVChanDataTypes(chanSet->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes); 
			} else {
				DLDataTypes		allowedTypes[] = {DL_Waveform_Double, DL_RepeatedWaveform_Double, DL_Waveform_Float, DL_RepeatedWaveform_Float};
				SetSinkVChanDataTypes(chanSet->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes);
			}
			
			// update device settings
			errChk ( ConfigDAQmxAOTask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AOVoltage_Range:
			
			GetCtrlVal(panel, control, &chanSet->chanAttr->Vrngs->selectedRange);
			
			chanSet->VMax = chanSet->chanAttr->Vrngs->high[chanSet->chanAttr->Vrngs->selectedRange];
			chanSet->VMin = chanSet->chanAttr->Vrngs->low[chanSet->chanAttr->Vrngs->selectedRange];
			
			// update device settings
			errChk ( ConfigDAQmxAOTask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AOVoltage_Terminal:
			
			int terminalType;
			
			GetCtrlVal(panel, control, &terminalType);
			chanSet->terminal = (Terminal_type) terminalType;
			
			// update device settings
			errChk ( ConfigDAQmxAOTask(chanSet->baseClass.device, &errorInfo.errMsg) );
			break;
			
		case AOVoltage_VChanName:
			
			char*	newName 	= NULL;
			
			newName = GetStringFromControl (panel, control);
			
			DLRenameVChan((VChan_type*)chanSet->baseClass.sinkVChan, newName);
			OKfree(newName);
			break;
	}
	
	return 0;
	
Error:
	
PRINT_ERR

	return 0;
}

//------------------------------------------------------------------------------
// ChanSet_AI_Current_type
//------------------------------------------------------------------------------

static ChanSet_AI_Current_type* init_ChanSet_AI_Current_type (Dev_type* dev, char physChanName[], AIChannel_type* chanAttr, BOOL integrateFlag, DataTypeConversion_type dataTypeConversion, 
																uInt32 IRangeIdx, ShuntResistor_type shuntResistorType, double shuntResistorValue, Terminal_type terminalType)
{
	ChanSet_AI_Current_type* chan = malloc (sizeof(ChanSet_AI_Current_type));
	if (!chan) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &chan->baseClass, physChanName, Chan_AI_Current, (DiscardFptr_type)discard_ChanSet_AI_Current_type);
	chan->baseClass.dataTypeConversion	= dataTypeConversion;
	chan->baseClass.integrateFlag		= integrateFlag;
	
	// init child
	chan->chanAttr						= chanAttr;
	chan->IMax							= chan->chanAttr->Irngs->high[IRangeIdx];
	chan->IMin							= chan->chanAttr->Irngs->low[IRangeIdx]; 
	chan->shuntResistorType				= shuntResistorType;
	chan->shuntResistorValue			= shuntResistorValue;
	chan->terminal						= terminalType;
	
	return chan;
	
}

static void discard_ChanSet_AI_Current_type (ChanSet_AI_Current_type** chanSetPtr)
{
	if (!*chanSetPtr) return;
	
	// discard base class
	discard_ChanSet_type((ChanSet_type**)chanSetPtr);
}

//------------------------------------------------------------------------------
// ChanSet_AO_Current_type
//------------------------------------------------------------------------------

static ChanSet_AO_Current_type* init_ChanSet_AO_Current_type (Dev_type* dev, char physChanName[], AOChannel_type* chanAttr, uInt32 IRangeIdx, ShuntResistor_type shuntResistorType, 
															  double shuntResistorValue, Terminal_type terminalType)
{
	ChanSet_AO_Current_type* chan = malloc (sizeof(ChanSet_AO_Current_type));
	if (!chan) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &chan->baseClass, physChanName, Chan_AO_Current, (DiscardFptr_type)discard_ChanSet_AO_Current_type);
	
	// init child
	chan->chanAttr						= chanAttr;
	chan->IMax							= chan->chanAttr->Irngs->high[IRangeIdx];
	chan->IMin							= chan->chanAttr->Irngs->low[IRangeIdx]; 
	chan->shuntResistorType				= shuntResistorType;
	chan->shuntResistorValue			= shuntResistorValue;
	chan->terminal						= terminalType;
	
	return chan;
	
}

static void discard_ChanSet_AO_Current_type (ChanSet_AO_Current_type** chanSetPtr)
{
	if (!*chanSetPtr) return;
	
	// discard base class
	discard_ChanSet_type((ChanSet_type**)chanSetPtr);
}

// Calculates offset and gain so that a signal S over a range of [Vmin, Vmax] is mapped to a signal S' over a range [Vmin', Vmax']
// The mapping is done according to the formula:
//
// S' = S * gain + offset
//
// where gain = (Vmax' - Vmin')/(Vmax - Vmin) and offset = - Vmin * gain + Vmin' and Vmax != Vmin
void AdjustAIDataTypeGainOffset (ChanSet_AI_Voltage_type* chSet, uInt32 oversampling)
{
	int 		AIDataTypeIdx	= 0;
	double  	maxSignalRange	= 0;
	
	GetCtrlIndex(chSet->baseClass.chanPanHndl, AIVoltage_AIDataType, &AIDataTypeIdx);
	
	switch (AIDataTypeIdx) {
			
		case Convert_To_Double:
		case Convert_To_Float:
			
			break;
			
		case Convert_To_UInt:
			
			maxSignalRange 								= (double) UINT_MAX;
			
			if (chSet->baseClass.dataTypeConversion.max != chSet->baseClass.dataTypeConversion.min)
				chSet->baseClass.dataTypeConversion.gain 	= maxSignalRange / ((chSet->baseClass.dataTypeConversion.max - chSet->baseClass.dataTypeConversion.min) * oversampling);
			else
				chSet->baseClass.dataTypeConversion.gain	= 0;
				
			chSet->baseClass.dataTypeConversion.offset		= - chSet->baseClass.dataTypeConversion.min * oversampling * chSet->baseClass.dataTypeConversion.gain;
			break;
			
		case Convert_To_UShort:
			
			maxSignalRange								= (double) USHRT_MAX;
			
			if (chSet->baseClass.dataTypeConversion.max != chSet->baseClass.dataTypeConversion.min)
				chSet->baseClass.dataTypeConversion.gain 	= maxSignalRange / ((chSet->baseClass.dataTypeConversion.max - chSet->baseClass.dataTypeConversion.min) * oversampling);
			else
				chSet->baseClass.dataTypeConversion.gain	= 0;
			
			chSet->baseClass.dataTypeConversion.offset		= - chSet->baseClass.dataTypeConversion.min * oversampling * chSet->baseClass.dataTypeConversion.gain;
			break;
			
		case Convert_To_UChar:
			
			maxSignalRange 								= (double) UCHAR_MAX;
			
			if (chSet->baseClass.dataTypeConversion.max != chSet->baseClass.dataTypeConversion.min)
				chSet->baseClass.dataTypeConversion.gain 	= maxSignalRange / ((chSet->baseClass.dataTypeConversion.max - chSet->baseClass.dataTypeConversion.min) * oversampling);
			else
				chSet->baseClass.dataTypeConversion.gain	= 0;
			
			chSet->baseClass.dataTypeConversion.offset		= - chSet->baseClass.dataTypeConversion.min * oversampling * chSet->baseClass.dataTypeConversion.gain;
			break;
	}
	
}

/// HIFN Sets AI data type conversion gain to 1.0 and offset to 0.0
void ResetAIDataTypeGainOffset (ChanSet_AI_Voltage_type* chSet)
{
	chSet->baseClass.dataTypeConversion.gain		= 1.0;
	chSet->baseClass.dataTypeConversion.offset		= 0.0; 
	
	SetCtrlVal(chSet->baseClass.chanPanHndl, AIVoltage_Gain, chSet->baseClass.dataTypeConversion.gain);  
	SetCtrlVal(chSet->baseClass.chanPanHndl, AIVoltage_Offset, chSet->baseClass.dataTypeConversion.offset);  
}

//------------------------------------------------------------------------------
// ChanSet_DIDO_type
//------------------------------------------------------------------------------
static ChanSet_DIDO_type* init_ChanSet_DIDO_type (Dev_type* dev, char physChanName[], Channel_type chanType, BOOL invert) 
{
	ChanSet_DIDO_type* chanSet = malloc (sizeof(ChanSet_DIDO_type));
	if (!chanSet) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &chanSet->baseClass, physChanName, chanType, (DiscardFptr_type)discard_ChanSet_DIDO_type);
	
	// init child
	chanSet->invert		= invert;
	
	return chanSet;
}

static void discard_ChanSet_DIDO_type (ChanSet_type** chanSetPtr)
{
	if (!*chanSetPtr) return; 
	
	// discard base class
	discard_ChanSet_type((ChanSet_type**)chanSetPtr);	
}

// DO channel UI callback for setting properties
static int ChanSetLineDO_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ChanSet_DIDO_type*  	chanset			= callbackData; 
	uInt32 					lineVal			= 0;  // line value

	if (event != EVENT_COMMIT) return 0;
	
	switch(control) {
		case DIDOLChSet_OnDemand:
			
			GetCtrlVal(panel, control, &chanset->baseClass.onDemand);
			// if this is an On Demand DO channel , adjust VChan such that only Waveform_type* data is allowed
			// otherwise allow both RepeatedWaveform_type* and Waveform_type* data
			if (chanset->baseClass.onDemand) {
				
				DLDataTypes allowedTypes[] = {	DL_Waveform_Char,
												DL_Waveform_UChar,
												DL_Waveform_Short,
												DL_Waveform_UShort,
												DL_Waveform_Int,
												DL_Waveform_UInt,
											 	DL_Waveform_Int64,
												DL_Waveform_UInt64};
						
				SetSinkVChanDataTypes(chanset->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes);
				
			} else {
				
				DLDataTypes allowedTypes[] = {	
												DL_Waveform_Char,
												DL_Waveform_UChar,
												DL_Waveform_Short,
												DL_Waveform_UShort,
												DL_Waveform_Int,
												DL_Waveform_UInt,
												DL_Waveform_Int64,
												DL_Waveform_UInt64,
												DL_RepeatedWaveform_Char,						
												DL_RepeatedWaveform_UChar,						
												DL_RepeatedWaveform_Short,						
												DL_RepeatedWaveform_UShort,					
												DL_RepeatedWaveform_Int,						
												DL_RepeatedWaveform_UInt,
												DL_RepeatedWaveform_Int64,
												DL_RepeatedWaveform_UInt64};
						
				SetSinkVChanDataTypes(chanset->baseClass.sinkVChan, NumElem(allowedTypes), allowedTypes);
			}
			
			// configure DO task
			errChk (ConfigDAQmxDOTask(chanset->baseClass.device, &errorInfo.errMsg) );   
			
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
			errChk(ConfigDAQmxDOTask(chanset->baseClass.device, &errorInfo.errMsg) );
			
			break;
			
		case DIDOLChSet_OutputBTTN:
			
			break;
			
	}
	
    GetCtrlVal(panel, DIDOLChSet_OutputBTTN, &lineVal);       
	
	// update output
 	errChk (DirectDigitalOut(chanset->baseClass.name, lineVal, chanset->invert, DAQmxDefault_Task_Timeout, &errorInfo.errMsg) );
	
	
Error:
	
PRINT_ERR
	
	return 0;
}

// DO channel UI callback for setting properties
static int ChanSetPortDO_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ChanSet_DIDO_type*  chanset 	= callbackData; 
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
			errChk(ConfigDAQmxDOTask(chanset->baseClass.device, &errorInfo.errMsg) );
			
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
 	errChk( DirectDigitalOut(chanset->baseClass.name, data, chanset->invert, DAQmxDefault_Task_Timeout, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return 0;
}

static int ReadDirectDigitalOut(char* chan, uInt32* data, char** errorMsg)
{
INIT_ERR

	TaskHandle  taskHandle	= 0;
	int32		nRead		= 0;

	DAQmxErrChk( DAQmxCreateTask("", &taskHandle) );
	DAQmxErrChk( DAQmxCreateDOChan(taskHandle, chan, "", DAQmx_Val_ChanForAllLines) );
	DAQmxErrChk( DAQmxStartTask(taskHandle) );
	DAQmxErrChk( DAQmxReadDigitalU32(taskHandle, 1, 10.0, DAQmx_Val_GroupByChannel, data, 1, &nRead, NULL) );
	DAQmxErrChk( DAQmxClearTask(taskHandle) );
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO
	
Error:
	
RETURN_ERR
}

static int DirectDigitalOut (char* chan,uInt32 data,BOOL invert, double timeout, char** errorMsg)
{
INIT_ERR

	TaskHandle  taskHandle	= 0;
	int32		nWritten	= 0;

	if (invert) data =~data;   //invert data
	
	DAQmxErrChk( DAQmxCreateTask("", &taskHandle) );
	DAQmxErrChk( DAQmxCreateDOChan(taskHandle, chan, "", DAQmx_Val_ChanForAllLines) );
	DAQmxErrChk( DAQmxStartTask(taskHandle) );
	DAQmxErrChk( DAQmxWriteDigitalU32(taskHandle, 1, 1, timeout, DAQmx_Val_GroupByChannel, &data, &nWritten, NULL) );
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR
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
// ChanSet_CI
//------------------------------------------------------------------------------

static void	init_ChanSet_CI_type (Dev_type* dev, ChanSet_CI_type* chanSet, char physChanName[], Channel_type chanType, EdgeTypes activeEdge, DiscardFptr_type discardFptr)
{
	// init base class
	init_ChanSet_type(dev, &chanSet->baseClass, physChanName, chanType, discardFptr);
	
	// init
	chanSet->taskHndl			= NULL;
	chanSet->activeEdge			= activeEdge;
	chanSet->startTrig			= NULL;
	chanSet->referenceTrig		= NULL;
	chanSet->HWTrigMaster		= NULL;
	chanSet->HWTrigSlave		= NULL;
}

static void discard_ChanSet_CI_type (ChanSet_CI_type** chanSetPtr)
{
	ChanSet_CI_type* chanSet = *chanSetPtr;
	if (!chanSet) return;
	
	// discard DAQmx task if any
	if (chanSet->taskHndl) {
		DAQmxTaskControl(chanSet->taskHndl, DAQmx_Val_Task_Abort);
		DAQmxClearTask (chanSet->taskHndl); 
		chanSet->taskHndl = 0;
	}
	
	// discard triggers if any
	discard_TaskTrig_type(&chanSet->startTrig);
	discard_TaskTrig_type(&chanSet->referenceTrig);
	
	// discard HW triggers
	discard_HWTrigMaster_type(&chanSet->HWTrigMaster);
	discard_HWTrigSlave_type(&chanSet->HWTrigSlave);
	
	// discard base class
	discard_ChanSet_type((ChanSet_type**)chanSetPtr);
}

//------------------------------------------------------------------------------
// ChanSet_CI_EdgeCount_type
//------------------------------------------------------------------------------

static ChanSet_CI_EdgeCount_type* init_ChanSet_CI_EdgeCount_type (Dev_type* dev, char physChanName[], EdgeTypes activeEdge, uInt32 initialCount, CountDirections countDirection)
{
	ChanSet_CI_EdgeCount_type* chanSet = malloc (sizeof(ChanSet_CI_EdgeCount_type));
	if (!chanSet) return NULL;
	
	// initialize base class
	init_ChanSet_CI_type(dev, &chanSet->baseClass, physChanName, Chan_CI_EdgeCount, activeEdge, (DiscardFptr_type)discard_ChanSet_CI_EdgeCount_type);   
	
	chanSet->initialCount		= initialCount;
	chanSet->countDirection		= countDirection;
	
	return chanSet;
}

static void discard_ChanSet_CI_EdgeCount_type (ChanSet_CI_EdgeCount_type** chanSetPtr)
{
	ChanSet_CI_EdgeCount_type* chanSet = *chanSetPtr; 
	if (!chanSet) return;
	
	// discard base class
	discard_ChanSet_CI_type((ChanSet_CI_type**)chanSetPtr);
}

// CI Edge channel UI callback for setting channel properties
static int ChanSetCIEdge_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	ChanSet_CI_EdgeCount_type* 	chSet 	= callbackData;
	char*						newName 	= NULL; 
	if (event != EVENT_COMMIT) return 0;
	
	switch(control) {
			
		case VChanPan_SrcVChanName:
			
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)chSet->baseClass.baseClass.srcVChan, newName);
			OKfree(newName);
			
			break;
			
		case VChanPan_SinkVChanName:
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)chSet->baseClass.baseClass.sinkVChan, newName);
			OKfree(newName);
			
			break;
	}
	
	return 0;
}


//------------------------------------------------------------------------------
// ChanSet_CI_Frequency_type
//------------------------------------------------------------------------------

static ChanSet_CI_Frequency_type* init_ChanSet_CI_Frequency_type (Dev_type* dev, char physChanName[], double freqMax, double freqMin, EdgeTypes activeEdge, CIFrequencyMeasMethods measMethod, 
																  double measTime, uInt32 divisor, char freqInputTerminal[], CounterTaskTiming_type taskTiming)
{
	ChanSet_CI_Frequency_type* chanSet = malloc (sizeof(ChanSet_CI_Frequency_type));
	if (!chanSet) return NULL;
	
	// initialize base class
	init_ChanSet_CI_type(dev, &chanSet->baseClass, physChanName, Chan_CI_Frequency, activeEdge, (DiscardFptr_type)discard_ChanSet_CI_Frequency_type);   
	
	// init child class
	chanSet->freqMax			= freqMax;
	chanSet->freqMin			= freqMin;
	chanSet->measMethod			= measMethod;
	chanSet->measTime			= measTime;
	chanSet->divisor 			= divisor;
	chanSet->freqInputTerminal  = StrDup(freqInputTerminal);
	chanSet->taskTiming			= taskTiming;
	
	return chanSet;
	
}

static void discard_ChanSet_CI_Frequency_type (ChanSet_CI_Frequency_type** chanSetPtr)
{
	ChanSet_CI_Frequency_type* chanSet = *chanSetPtr; 
	if (!chanSet) return; 
	
	OKfree(chanSet->freqInputTerminal);
	
	// discard base class
	discard_ChanSet_CI_type((ChanSet_CI_type**)chanSetPtr);
}

//------------------------------------------------------------------------------
// ChanSet_CO_type
//------------------------------------------------------------------------------
static ChanSet_CO_type* init_ChanSet_CO_type (Dev_type* dev, char physChanName[], Channel_type chanType, char clockSource[], PulseTrain_type* pulseTrain)
{
	ChanSet_CO_type*	chanSet = malloc (sizeof(ChanSet_CO_type));
	if (!chanSet) return NULL;
	
	// initialize base class
	init_ChanSet_type(dev, &chanSet->baseClass, physChanName, chanType, (DiscardFptr_type)discard_ChanSet_CO_type);
	
	// init child class
	chanSet->taskHndl		= NULL;
	chanSet->startTrig		= NULL;
	chanSet->referenceTrig	= NULL;
	chanSet->HWTrigMaster	= NULL;
	chanSet->HWTrigSlave	= NULL;
	chanSet->pulseTrain 	= pulseTrain;
	chanSet->clockSource	= StrDup(clockSource);
	
	return chanSet;
}

static void discard_ChanSet_CO_type (ChanSet_CO_type** chanSetPtr)
{
	ChanSet_CO_type* chanSet = *chanSetPtr;
	if (!chanSet) return; 
	
	// discard DAQmx task if any
	if (chanSet->taskHndl) {
		DAQmxTaskControl(chanSet->taskHndl, DAQmx_Val_Task_Abort);
		DAQmxClearTask (chanSet->taskHndl); 
		chanSet->taskHndl = 0;
	}
	
	// discard triggers if any
	discard_TaskTrig_type(&chanSet->startTrig);
	discard_TaskTrig_type(&chanSet->referenceTrig);
	
	// discard HW triggers
	discard_HWTrigMaster_type(&chanSet->HWTrigMaster);
	discard_HWTrigSlave_type(&chanSet->HWTrigSlave);
	
	discard_PulseTrain_type (&chanSet->pulseTrain);
	
	// discard base class
	discard_ChanSet_type((ChanSet_type**)chanSetPtr);
}

// CO channel UI callback for setting properties
static int ChanSetCO_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
//INIT_ERR	
	
	ChanSet_CO_type*	COChan 			= callbackData;
	char*				newName 		= NULL;
	
	
	if (event != EVENT_COMMIT) return 0;	// do nothing
	
	switch(control) {
			
		case VChanPan_SrcVChanName:
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)COChan->baseClass.srcVChan, newName);
			OKfree(newName);
			
			break;
			
		case VChanPan_SinkVChanName:
			
			newName = GetStringFromControl (panel, control);
			DLRenameVChan((VChan_type*)COChan->baseClass.sinkVChan, newName);
			OKfree(newName);
			
			break;
	}
	
	return 0;
}

/// HIFN Obtains a list of physical channel properties for a given DAQmx channel attribute that returns an array of properties.
/// HIPAR chanName/ Physical channel name, e.g. dev1/ai0
/// HIPAR chanIOAttribute/ Physical channel property returning an array such as: DAQmx_PhysicalChan_AI_SupportedMeasTypes, DAQmx_PhysicalChan_AO_SupportedOutputTypes,etc.
static ListType	GetPhysChanPropertyList	(char chanName[], int chanProperty)
{
	ListType 	propertyList 	= ListCreate(sizeof(int));
	int32		nElem			= 0;
	int*		properties		= NULL;
	
	if (!propertyList) return 0;
	
	nElem = DAQmxGetPhysicalChanAttribute(chanName, chanProperty, NULL); 
	if (!nElem) return propertyList;
	if (nElem < 0) goto Error;
	
	properties = malloc (nElem * sizeof(int));
	if (!properties) goto Error; // also if nElem = 0, i.e. no properties found
	
	if (DAQmxGetPhysicalChanAttribute(chanName, chanProperty, properties, nElem) < 0) goto Error;
	
	for (int32 i = 0; i < nElem; i++)
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
	int							ioVal			= 0;
	int							ioMode			= 0;
	int							ioType			= 0;
	char*						shortChName		= NULL;
	AIChannel_type*				AIChan			= NULL;
	AOChannel_type*   			AOChan			= NULL;
	DILineChannel_type*			DILineChan		= NULL;
	DIPortChannel_type*  		DIPortChan		= NULL;
	DOLineChannel_type*			DOLineChan		= NULL;
	DOPortChannel_type*  		DOPortChan		= NULL;
	CIChannel_type*				CIChan			= NULL;
	COChannel_type*   			COChan			= NULL;
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
					for (size_t i = 1; i <= n; i++) {
						AIChan = *(AIChannel_type**)ListGetPtrToItem(dev->attr->AIchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem (AIChan->supportedMeasTypes, &ioType, FRONT_OF_LIST, IntCompare) && !AIChan->inUse) {
							shortChName = strstr(AIChan->physChanName, "/") + 1;  
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, AIChan->physChanName);
						}
					}
					
					break;
					
				case DAQmxDigital:
					
					switch (ioType) {
						
						case Chan_DI_Line:
							
							n = ListNumItems(dev->attr->DIlines);
							for (size_t i = 1; i <= n; i++) {
								DILineChan = *(DILineChannel_type**)ListGetPtrToItem(dev->attr->DIlines, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!DILineChan->inUse) {
									shortChName = strstr(DILineChan->physChanName, "/") + 1;  
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, DILineChan->physChanName);
								}
							}
							
							break;
							
						case Chan_DI_Port:
							
							n = ListNumItems(dev->attr->DIports);
							for (size_t i = 1; i <= n; i++) {
								DIPortChan = *(DIPortChannel_type**)ListGetPtrToItem(dev->attr->DIports, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!DIPortChan->inUse) {
									shortChName = strstr(DIPortChan->physChanName, "/") + 1;  
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, DIPortChan->physChanName);
								}
							}
							
							break;
					}
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->CIchan);
					for (size_t i = 1; i <= n; i++) {
						CIChan = *(CIChannel_type**)ListGetPtrToItem(dev->attr->CIchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem (CIChan->supportedMeasTypes, &ioType, FRONT_OF_LIST, IntCompare) && !CIChan->inUse) {
							shortChName = strstr(CIChan->physChanName, "/") + 1; 
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, CIChan->physChanName);
						}
					}
					
					break;
			}
			
			break;
			
		case DAQmxGenerate:
			
			switch (ioMode) {
				
				case DAQmxAnalog:
					
					n = ListNumItems(dev->attr->AOchan);
					for (size_t i = 1; i <= n; i++) {
						AOChan = *(AOChannel_type**)ListGetPtrToItem(dev->attr->AOchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem (AOChan->supportedOutputTypes, &ioType, FRONT_OF_LIST, IntCompare) && !AOChan->inUse) {
							shortChName = strstr(AOChan->physChanName, "/") + 1;
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, AOChan->physChanName);
						}
					}
					
					break;
					
				case DAQmxDigital:
					
						switch (ioType) {
						
						case Chan_DO_Line:
							
							n = ListNumItems(dev->attr->DOlines);
							for (size_t i = 1; i <= n; i++) {
								DOLineChan = *(DOLineChannel_type**)ListGetPtrToItem(dev->attr->DOlines, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!DOLineChan->inUse) {
									shortChName = strstr(DOLineChan->physChanName, "/") + 1;
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, DOLineChan->physChanName);
								}
							}
							
							break;
							
						case Chan_DO_Port:
							
							n = ListNumItems(dev->attr->DOports);
							for (size_t i = 1; i <= n; i++) {
								DOPortChan = *(DOPortChannel_type**)ListGetPtrToItem(dev->attr->DOports, i);
								// select channels that have not been already added to the DAQmx tasks
								if (!DOPortChan->inUse) {
									shortChName = strstr(DOPortChan->physChanName, "/") + 1;
									// insert physical channel name in the list
									InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, DOPortChan->physChanName);
								}
							}
							
							break;
					}
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->COchan);
					for (size_t i = 1; i <= n; i++) {
						COChan = *(COChannel_type**)ListGetPtrToItem(dev->attr->COchan, i);
						// select only channels that support this measurement type since not all channels of the same type may have the same capabilities
						// as well as select channels that have not been already added to the DAQmx tasks
						if (ListFindItem (COChan->supportedOutputTypes, &ioType, FRONT_OF_LIST, IntCompare) && !COChan->inUse) {
							shortChName = strstr(COChan->physChanName, "/") + 1;
							// insert physical channel name in the list
							InsertListItem(dev->devPanHndl, TaskSetPan_PhysChan, -1, shortChName, COChan->physChanName);
						}
					}
					
					break;
			}
			
			break;
	}
	
}

static int AddDAQmxChannel (Dev_type* dev, DAQmxIO_type ioVal, DAQmxIOMode_type ioMode, int ioType, char chName[], char** errorMsg)
{
#define AddDAQmxChannel_Err_TaskConfigFailed	-1

INIT_ERR
	
	char* 		newVChanName	= NULL;	
	int 		panHndl			= 0;
	int    		newTabIdx		= 0;
	void*   	callbackData	= NULL; 
	int 		chanPanHndl		= 0; 
	char* 		shortChanName	= NULL;
	
	switch (ioVal) {
		
		case DAQmxAcquire: 
			
			switch (ioMode) {
			
				case DAQmxAnalog:
					
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
					AIChannel_type* AIChanAttr 	= GetAIChannel(dev, chName);
					
					// mark channel as in use
					AIChanAttr->inUse = TRUE;
							
					switch (ioType) {
								
						case DAQmx_Val_Voltage:			
							
							// init new channel data
							DataTypeConversion_type	AIDataTypeConversion = {.dataType = Convert_To_Double, .min = AIChanAttr->Vrngs->low[AIChanAttr->Vrngs->Nrngs - 1], 
																			.max = AIChanAttr->Vrngs->high[AIChanAttr->Vrngs->Nrngs - 1], .offset = 0, .gain = 1};
							ChanSet_AI_Voltage_type* newChan = init_ChanSet_AI_Voltage_type(dev, chName, AIChanAttr, FALSE, AIDataTypeConversion, AIChanAttr->Vrngs->Nrngs - 1, Terminal_None);
							// add new AI channel to UI
							AddToUI_Chan_AI_Voltage(newChan);
							// add new AI channel to list of channels
							ListInsertItem(dev->AITaskSet->chanSet, &newChan, END_OF_LIST);
							break;
							 
							// add here more channel cases
					}
					
					//--------------------------------------  
					// Configure AI task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxAITask(dev, &errorInfo.errMsg) );
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
							
					errChk( ConfigDAQmxDITask(dev, &errorInfo.errMsg) );
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
						dev->CITaskSet = (CITaskSet_type*) init_CITaskSet_type(dev);    
							
						// load UI resources
						int CICOTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOTskSet);  
						// insert panel to UI and keep track of the AI task settings panel handle
						newTabIdx = InsertPanelAsTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, CICOTaskSetPanHndl); 
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, &dev->CITaskSet->panHndl);
						// change tab title to new Task Controller name
						SetTabPageAttribute(dev->devPanHndl, TaskSetPan_DAQTasks, newTabIdx, ATTR_LABEL_TEXT, DAQmxCITaskSetTabName);
						
						// remove "None" labelled task settings tab (always first tab) if its panel doesn't have callback data attached to it  
						GetPanelHandleFromTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, &panHndl);
						GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &callbackData); 
						if (!callbackData) DeleteTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, 0, 1);
						// connect AI task settings data to the panel
						SetPanelAttribute(dev->CITaskSet->panHndl, ATTR_CALLBACK_DATA, dev->CITaskSet);
							
						//--------------------------
						// adjust "Channels" tab
						//--------------------------
						// get channels tab panel handle
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
						CounterTaskTiming_type			counterTaskTiming = {.measMode = Operation_Finite, .nSamples = DAQmxDefault_CO_Task_nPulses, .sampleRate = DAQmxDefault_Task_SampleRate, .refClkFreq = DAQmxDefault_Task_RefClkFreq};	
						ChanSet_CI_EdgeCount_type*	 	newChan 	=  (ChanSet_CI_EdgeCount_type*) init_ChanSet_CI_Frequency_type(dev, chName, DAQmxDefault_CI_Frequency_Task_MaxFreq, DAQmxDefault_CI_Frequency_Task_MinFreq,
																														  DAQmxDefault_CI_Frequency_Task_Edge, DAQmxDefault_CI_Frequency_Task_MeasMethod, DAQmxDefault_CI_Frequency_Task_MeasTime, 
																														  DAQmxDefault_CI_Frequency_Task_Divisor, NULL, counterTaskTiming);
							
						// insert new channel settings tab
						int chanSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOChSet);  
						newTabIdx = InsertPanelAsTabPage(chanPanHndl, Chan_ChanSet, -1, chanSetPanHndl); 
						// change tab title
						shortChanName = strstr(chName, "/") + 1;  
						SetTabPageAttribute(chanPanHndl, Chan_ChanSet, newTabIdx, ATTR_LABEL_TEXT, shortChanName); 
						DiscardPanel(chanSetPanHndl); 
						chanSetPanHndl = 0;
						GetPanelHandleFromTabPage(chanPanHndl, Chan_ChanSet, newTabIdx, &newChan->baseClass.baseClass.chanPanHndl);
						// add callbackdata to the channel panel
							
						SetPanelAttribute(newChan->baseClass.baseClass.chanPanHndl, ATTR_CALLBACK_DATA, newChan);
						// add callback data to the controls in the panel
						//	SetCtrlsInPanCBInfo(newChan, ChanSetAIAOVoltage_CB, newChan->baseClass.chanPanHndl);
						
						//--------------------------
						// Create and register VChan
						//--------------------------
							
						newChan->baseClass.baseClass.srcVChan = init_SourceVChan_type(newVChanName, DL_Waveform_Double, newChan, NULL);  
						DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newChan->baseClass.baseClass.srcVChan);
						SetCtrlVal(newChan->baseClass.baseClass.chanPanHndl, VChanPan_SrcVChanName, newVChanName);
						OKfree(newVChanName);
							
						//---------------------------------------
						// Add new CI channel to list of channels
						//---------------------------------------
						ListInsertItem(dev->CITaskSet->chanTaskSet, &newChan, END_OF_LIST);
			
					}
					
					//--------------------------------------  
					// Configure CI tasks on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxCITask(dev, &errorInfo.errMsg) );
					
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
					
					AOChannel_type* AOChanAttr 	= GetAOChannel(dev, chName);
				
					// mark channel as in use
					AOChanAttr->inUse = TRUE;
							
					GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_ChanTabIdx, &chanPanHndl);
					
					switch (ioType) {
						
						case DAQmx_Val_Voltage:
							
							// init new channel data
							DataTypeConversion_type	AIDataTypeConversion = {.dataType = Convert_To_Double, .min = AOChanAttr->Vrngs->low[AOChanAttr->Vrngs->Nrngs - 1], 
																			.max = AOChanAttr->Vrngs->high[AOChanAttr->Vrngs->Nrngs - 1], .offset = 0, .gain = 1}; 
							ChanSet_AO_Voltage_type* newChan = init_ChanSet_AO_Voltage_type(dev, chName, AOChanAttr, AOChanAttr->Vrngs->Nrngs - 1, Terminal_None);
							// add new AO channel to UI
							AddToUI_Chan_AO_Voltage(newChan);
							// add new AO channel to list of channels
							ListInsertItem(dev->AOTaskSet->chanSet, &newChan, END_OF_LIST);
							break;
							
							// add here more channel types
					}
					
					//--------------------------------------  
					// Configure AO task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxAOTask(dev, &errorInfo.errMsg) );
					
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
					     
					ChanSet_DIDO_type* 	newDOChan		= NULL;
					
					GetCtrlVal(dev->devPanHndl, TaskSetPan_IOType, &ioType); 
					
					//set channel type 
					switch (ioType){
						
						case Chan_DO_Line:
						
							newDOChan = init_ChanSet_DIDO_type (dev, chName, ioType, FALSE);  
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
							errChk( ReadDirectDigitalOut(newDOChan->baseClass.name, &data, &errorInfo.errMsg) );
							SetCtrlVal(newDOChan->baseClass.chanPanHndl,DIDOLChSet_OutputBTTN,data);           
							break;
								
						case Chan_DO_Port:
							
							newDOChan =init_ChanSet_DIDO_type (dev, chName, ioType, FALSE);  
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
							errChk( ReadDirectDigitalOut(newDOChan->baseClass.name, &data, &errorInfo.errMsg) );
							SetPortControls(newDOChan->baseClass.chanPanHndl,data);  
						
							break;
					}
				    
					
					// insert new channel settings tab
					
					//-------------------------------------------------------------
					// Create and register VChan with Task Controller and framework
					//-------------------------------------------------------------
							
					DLDataTypes allowedPacketTypes[] = { DL_Waveform_Char,
													  	 DL_Waveform_UChar,
														 DL_Waveform_Short,
								 						 DL_Waveform_UShort,
														 DL_Waveform_Int,
								    					 DL_Waveform_UInt,
														 DL_Waveform_Int64,
								    					 DL_Waveform_UInt64,
									   					 DL_RepeatedWaveform_Char,						
													 	 DL_RepeatedWaveform_UChar,						
														 DL_RepeatedWaveform_Short,						
														 DL_RepeatedWaveform_UShort,					
														 DL_RepeatedWaveform_Int,						
														 DL_RepeatedWaveform_UInt,
												   		 DL_RepeatedWaveform_Int64,						
														 DL_RepeatedWaveform_UInt64};
						
					newDOChan->baseClass.sinkVChan = init_SinkVChan_type(newVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), newDOChan, VChan_Data_Receive_Timeout, NULL);  
					DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)newDOChan->baseClass.sinkVChan);
					
					errChk( AddSinkVChan(dev->taskController, newDOChan->baseClass.sinkVChan, DO_DataReceivedTC, &errorInfo.errMsg) ); 
					OKfree(newVChanName);
							
					//---------------------------------------
					// Add new DO channel to list of channels
					//---------------------------------------
					ListInsertItem(dev->DOTaskSet->chanSet, &newDOChan, END_OF_LIST);
				
					//--------------------------------------  
					// Configure DO task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxDOTask(dev, &errorInfo.errMsg) );
					
					break;
					
				case DAQmxCounter:
					
					//-------------------------------------------------
					// if there is no CO task then create it and add UI
					//-------------------------------------------------
							
					if(!dev->COTaskSet) {
						// init CO task structure data
						dev->COTaskSet = init_COTaskSet_type(dev);    
						
						// load UI resources
						int CICOTaskSetPanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, CICOTskSet);  
						// insert panel to UI and keep track of the AI task settings panel handle
						newTabIdx = InsertPanelAsTabPage(dev->devPanHndl, TaskSetPan_DAQTasks, -1, CICOTaskSetPanHndl); 
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
							
					Channel_type 				chanType		= 0;
					PulseTrain_type*			pulseTrain		= NULL;
					
					GetCtrlVal(dev->devPanHndl, TaskSetPan_IOType, &ioType);  
					//set channel type 
					switch (ioType){
						
						case DAQmx_Val_Pulse_Freq:
							chanType		= Chan_CO_Pulse_Frequency;
							pulseTrain		= (PulseTrain_type*) init_PulseTrainFreqTiming_type(PulseTrain_Finite, DAQmxDefault_CO_Task_idlestate, DAQmxDefault_CO_Task_nPulses, 
									  							 DAQmxDefault_CO_Frequency_Task_dutycycle, DAQmxDefault_CO_Frequency_Task_freq, DAQmxDefault_CO_Frequency_Task_initdelay); 
							break;
									
						case DAQmx_Val_Pulse_Time:
							chanType		= Chan_CO_Pulse_Time;
							pulseTrain		= (PulseTrain_type*) init_PulseTrainTimeTiming_type(PulseTrain_Finite, DAQmxDefault_CO_Task_idlestate, DAQmxDefault_CO_Task_nPulses, 
									  							 DAQmxDefault_CO_Time_Task_hightime, DAQmxDefault_CO_Time_Task_lowtime, DAQmxDefault_CO_Time_Task_initdelay); 
							break;
									
						case DAQmx_Val_Pulse_Ticks:
							chanType		= Chan_CO_Pulse_Ticks;
							pulseTrain		= (PulseTrain_type*) init_PulseTrainTickTiming_type(PulseTrain_Finite, DAQmxDefault_CO_Task_idlestate, DAQmxDefault_CO_Task_nPulses,
									  							 DAQmxDefault_CO_Ticks_Task_highticks, DAQmxDefault_CO_Ticks_Task_lowticks, DAQmxDefault_CO_Ticks_Task_initdelayticks);  
							break;
					}
					
					
					ChanSet_CO_type* newChan 	=  init_ChanSet_CO_type(dev, chName, chanType, NULL, pulseTrain);
						
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
					//GetCtrlVal(settingsPanHndl, TimingPan_Timeout, &dev->COTaskSet->timeout);
						
					// add callback to controls in the panel
					SetCtrlsInPanCBInfo(newChan, ChanSetCO_CB, settingsPanHndl);   
								
					//--------------------------
					// adjust "Pulse" tab
					//--------------------------
					
					// delete empty Pulse tab
					DeleteTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, 1); 
					
					int 					pulsePanHndl	= 0;	
					int 					COPulsePanHndl	= 0;
					PulseTrainIdleStates	idleState		= 0;
					int 					ctrlIdx			= 0;
					
					switch (chanType){
						
						case Chan_CO_Pulse_Frequency:
							// load panel
							COPulsePanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, COFreqPan);
							InsertPanelAsTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, COPulsePanHndl);
							DiscardPanel(COPulsePanHndl);
							SetTabPageAttribute(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, ATTR_LABEL_TEXT, "Pulse");
							GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &pulsePanHndl);
							
							// add control callbacks
							SetCtrlsInPanCBInfo(newChan, Chan_CO_Pulse_Frequency_CB, pulsePanHndl);
							
							// init idle state
							idleState = GetPulseTrainIdleState(newChan->pulseTrain);
							switch (idleState) {
								
								case PulseTrainIdle_Low:
									SetCtrlIndex(pulsePanHndl, COFreqPan_IdleState, 0);
									break;
								case PulseTrainIdle_High:
									SetCtrlIndex(pulsePanHndl, COFreqPan_IdleState, 1);
									break;
							}
							
							// init frequency
							SetCtrlVal(pulsePanHndl, COFreqPan_Frequency, GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)newChan->pulseTrain)); 					// display in [Hz]
							
							// init duty cycle
							SetCtrlVal(pulsePanHndl, COFreqPan_DutyCycle, GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)newChan->pulseTrain)); 				// display in [%]
							
							// init initial delay
							SetCtrlVal(pulsePanHndl, COFreqPan_InitialDelay, GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)newChan->pulseTrain)*1e3); 	// display in [ms]
							
							// init CO generation mode
							InsertListItem(pulsePanHndl, COFreqPan_Mode, -1, "Finite", Operation_Finite);
							InsertListItem(pulsePanHndl, COFreqPan_Mode, -1, "Continuous", Operation_Continuous);
							GetIndexFromValue(pulsePanHndl, COFreqPan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(GetPulseTrainMode(newChan->pulseTrain))); 
							SetCtrlIndex(pulsePanHndl, COFreqPan_Mode, ctrlIdx);
							
							// init CO N Pulses
							SetCtrlVal(pulsePanHndl, COFreqPan_NPulses, GetPulseTrainNPulses(newChan->pulseTrain)); 
							
							break;
									
						case Chan_CO_Pulse_Time:
							
							COPulsePanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, COTimePan);
							InsertPanelAsTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, COPulsePanHndl);
							DiscardPanel(COPulsePanHndl);
							SetTabPageAttribute(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, ATTR_LABEL_TEXT, "Pulse");
							GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &pulsePanHndl);
							
							// add control callbacks
							SetCtrlsInPanCBInfo(newChan, Chan_CO_Pulse_Time_CB, pulsePanHndl);
							
							// init idle state
							idleState = GetPulseTrainIdleState(newChan->pulseTrain);
							switch (idleState) {
								
								case PulseTrainIdle_Low:
									SetCtrlIndex(pulsePanHndl, COTimePan_IdleState, 0);
									break;
								case PulseTrainIdle_High:
									SetCtrlIndex(pulsePanHndl, COTimePan_IdleState, 1);
									break;
							}
							
							// init high time
							SetCtrlVal(pulsePanHndl, COTimePan_HighTime, GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)newChan->pulseTrain)*1e3); 			// display in [ms]
							
							// init low time
							SetCtrlVal(pulsePanHndl, COTimePan_LowTime, GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)newChan->pulseTrain)*1e3); 				// display in [ms]
							
							// init initial delay
							SetCtrlVal(pulsePanHndl, COTimePan_InitialDelay, GetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)newChan->pulseTrain)*1e3); 	// display in [ms]
							
							// init CO generation mode
							InsertListItem(pulsePanHndl, COTimePan_Mode, -1, "Finite", Operation_Finite);
							InsertListItem(pulsePanHndl, COTimePan_Mode, -1, "Continuous", Operation_Continuous);
							GetIndexFromValue(pulsePanHndl, COTimePan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(GetPulseTrainMode(newChan->pulseTrain))); 
							SetCtrlIndex(pulsePanHndl, COTimePan_Mode, ctrlIdx);
							
							// init CO N Pulses
							SetCtrlVal(pulsePanHndl, COTimePan_NPulses, GetPulseTrainNPulses(newChan->pulseTrain)); 
									
							break;
									
						case Chan_CO_Pulse_Ticks:
							
							COPulsePanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, COTicksPan);
							InsertPanelAsTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, COPulsePanHndl);
							DiscardPanel(COPulsePanHndl);
							SetTabPageAttribute(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, ATTR_LABEL_TEXT, "Pulse");
							GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &pulsePanHndl);
							
							// add control callbacks
							SetCtrlsInPanCBInfo(newChan, Chan_CO_Pulse_Ticks_CB, pulsePanHndl);
							
							// init idle state
							idleState = GetPulseTrainIdleState(newChan->pulseTrain);
							switch (idleState) {
								
								case PulseTrainIdle_Low:
									SetCtrlIndex(pulsePanHndl, COTicksPan_IdleState, 0);
									break;
								case PulseTrainIdle_High:
									SetCtrlIndex(pulsePanHndl, COTicksPan_IdleState, 1);
									break;
							}
							
							// init high ticks
							SetCtrlVal(pulsePanHndl, COTicksPan_HighTicks, GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)newChan->pulseTrain));
							
							// init low ticks
							SetCtrlVal(pulsePanHndl, COTicksPan_LowTicks, GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)newChan->pulseTrain));
							
							// init initial delay ticks
							SetCtrlVal(pulsePanHndl, COTicksPan_InitialDelay, GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)newChan->pulseTrain));   
							
							// init CO generation mode
							InsertListItem(pulsePanHndl, COTicksPan_Mode, -1, "Finite", Operation_Finite);
							InsertListItem(pulsePanHndl, COTicksPan_Mode, -1, "Continuous", Operation_Continuous);
							GetIndexFromValue(pulsePanHndl, COTicksPan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(GetPulseTrainMode(newChan->pulseTrain))); 
							SetCtrlIndex(pulsePanHndl, COTicksPan_Mode, ctrlIdx);
							
							// init CO N Pulses
							SetCtrlVal(pulsePanHndl, COTicksPan_NPulses, GetPulseTrainNPulses(newChan->pulseTrain)); 
							
							break;
					}

					//--------------------------
					// adjust "Timing" tab
					//--------------------------
					// get timing tab panel handle
						
					int 	timingPanHndl = 0;
					
					GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_ClkTabIdx, &timingPanHndl);
					
					// add callback to controls in the panel
					// note: this must be done before changing the string control to a terminal control!
					SetCtrlsInPanCBInfo(newChan, Chan_CO_Clk_CB, timingPanHndl);
					
					// make sure that the host controls are not dimmed before inserting terminal controls!
					NIDAQmx_NewTerminalCtrl(timingPanHndl, TimingPan_ClockSource, 0); // single terminal selection
					
					// adjust sample clock terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, TimingPan_ClockSource, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
					NIDAQmx_SetTerminalCtrlAttribute(timingPanHndl, TimingPan_ClockSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
					// if CO channel type is ticks then undim clock source control, and dim it otherwise (do this after inserting the custom control)
					if (chanType == Chan_CO_Pulse_Ticks)
						SetCtrlAttribute(timingPanHndl, TimingPan_ClockSource, ATTR_DIMMED, FALSE);
					else
						SetCtrlAttribute(timingPanHndl, TimingPan_ClockSource, ATTR_DIMMED, TRUE);
						
					//--------------------------
					// adjust "Trigger" tab
					//--------------------------
					
					// get trigger tab panel handle
					newChan->startTrig = init_TaskTrig_type(dev, 0);  //? lex  
						
					int trigPanHndl;
					GetPanelHandleFromTabPage(newChan->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TriggerTabIdx, &trigPanHndl);
					
					// add callback to controls in the panel
					// note: this must be done before changing the string control to a terminal control!
					SetCtrlsInPanCBInfo(newChan, Chan_CO_Trig_CB, trigPanHndl);
					
					// make sure that the host controls are not dimmed before inserting terminal controls!
					NIDAQmx_NewTerminalCtrl(trigPanHndl, TrigPan_Source, 0); // single terminal selection
					
					// adjust sample clock terminal control properties
					NIDAQmx_SetTerminalCtrlAttribute(trigPanHndl, TrigPan_Source, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
					NIDAQmx_SetTerminalCtrlAttribute(trigPanHndl, TrigPan_Source, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
						
					// insert trigger type options
					InsertListItem(trigPanHndl, TrigPan_TrigType, -1, "None", Trig_None); 
					InsertListItem(trigPanHndl, TrigPan_TrigType, -1, "Digital Edge", Trig_DigitalEdge); 
					newChan->startTrig->trigType=Trig_None;
					SetCtrlAttribute(trigPanHndl,TrigPan_Slope,ATTR_DIMMED,TRUE);
					SetCtrlAttribute(trigPanHndl,TrigPan_Source,ATTR_DIMMED,TRUE); 									   ;  
						
					InsertListItem(trigPanHndl,TrigPan_Slope , 0, "Rising", TrigSlope_Rising); 
					InsertListItem(trigPanHndl,TrigPan_Slope , 1, "Falling", TrigSlope_Falling);
					newChan->startTrig->slope = TrigSlope_Rising;
						
					
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
															  						COPulseTrainSourceVChan_StateChange); 
							break;
								
						case PulseTrain_Time:
							newChan->baseClass.srcVChan		= init_SourceVChan_type(pulseTrainSourceVChanName, DL_PulseTrain_Time, newChan, 
														  						COPulseTrainSourceVChan_StateChange); 
							break;
								
						case PulseTrain_Ticks:
							newChan->baseClass.srcVChan 	= init_SourceVChan_type(pulseTrainSourceVChanName, DL_PulseTrain_Ticks, newChan, 
																  						COPulseTrainSourceVChan_StateChange); 
							break;
							
					}
					
					SetCtrlVal(settingspanel, VChanPan_SrcVChanName, pulseTrainSourceVChanName);  	
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
																					 newChan, VChan_Data_Receive_Timeout, COPulseTrainSinkVChan_StateChange); 
					
					SetCtrlVal(settingspanel, VChanPan_SinkVChanName, pulseTrainSinkVChanName);  
					// register VChan with DAQLab
					DLRegisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) newChan->baseClass.sinkVChan);	
					errChk( AddSinkVChan(dev->taskController, newChan->baseClass.sinkVChan, CO_DataReceivedTC, &errorInfo.errMsg) ); 
					
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
					newChan->HWTrigMaster 	= init_HWTrigMaster_type(HWTrigName);
					newChan->HWTrigSlave	= init_HWTrigSlave_type(HWTrigName);
					OKfree(HWTrigName);
						
					// register HW Triggers with framework
					DLRegisterHWTrigMaster(newChan->HWTrigMaster);
					DLRegisterHWTrigSlave(newChan->HWTrigSlave);
					
					//---------------------------------------
					// Add new CO channel to list of channels
					//---------------------------------------
					ListInsertItem(dev->COTaskSet->chanTaskSet, &newChan, END_OF_LIST);
					
					//--------------------------------------  
					// Configure CO task on device
					//-------------------------------------- 
							
					errChk( ConfigDAQmxCOTask(dev, &errorInfo.errMsg) );
					
					break;
			
				case DAQmxTEDS:
			
					break;
			}
		}
	
Error:

RETURN_ERR	
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
					(*(*chanSetPtr)->discardFptr)	((void**)chanSetPtr);
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
INIT_ERR
	
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
					// detach from Task Controller										 
					errChk( RemoveSinkVChan(dev->taskController, (*chanSetPtr)->sinkVChan, &errorInfo.errMsg) );
					// remove from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->sinkVChan);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	((void**)chanSetPtr);
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
	
Error:
	
PRINT_ERR

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
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)(*chanSetPtr)->srcVChan);
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	((void**)chanSetPtr);
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
INIT_ERR
	
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
					errChk( RemoveSinkVChan(dev->taskController, (*chanSetPtr)->sinkVChan, &errorInfo.errMsg) );
					// discard channel data structure
					(*(*chanSetPtr)->discardFptr)	((void**)chanSetPtr);
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
	
Error:

PRINT_ERR

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
INIT_ERR
	
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
			ChanSet_CO_type* 	chanSet		= NULL;
			ChanSet_CO_type** 	chanSetPtr	= NULL;
			size_t				nChans		= ListNumItems(dev->COTaskSet->chanTaskSet);
			for (size_t i = 1; i <= nChans; i++) {	
				chanSetPtr = ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
				chanSet = *chanSetPtr;
				
				if (chanSet == coChan) {
					// remove Source VChan from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)chanSet->baseClass.srcVChan);
					// remove Sink VChan from framework
					DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*)chanSet->baseClass.sinkVChan);
					// detach from Task Controller										 
					errChk( RemoveSinkVChan(dev->taskController, chanSet->baseClass.sinkVChan, &errorInfo.errMsg) );
					// remove HW triggers from framework
					DLUnregisterHWTrigMaster(chanSet->HWTrigMaster);
					DLUnregisterHWTrigSlave(chanSet->HWTrigSlave);
					// discard channel data structure
					(*chanSet->baseClass.discardFptr)	((void**)chanSetPtr);
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
	
Error:
	
PRINT_ERR

	return 0;
}

static ListType GetSupportedIOTypes (char devName[], int IOType)
{
	ListType 	IOTypes 	= ListCreate(sizeof(int));
	int32		nElem		= 0;
	int*		io			= NULL;
	
	if (!IOTypes) return 0;
	
	if((nElem = DAQmxGetDeviceAttribute(devName, IOType, NULL)) < 0) goto Error;
	
	if (!nElem) return IOTypes; // no suported IO types
	
	io = malloc (nElem * sizeof(int));
	if (!io) goto Error; // also if nElem = 0, i.e. no IO types found
	
	if (DAQmxGetDeviceAttribute(devName, IOType, io, nElem) < 0) goto Error;
	
	for (int32 i = 0; i < nElem; i++)
		ListInsertItem(IOTypes, &io[i], END_OF_LIST);
	OKfree(io);
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
								
							// Voltage measurement with an excitation source. 
							// You can use this measurement type for custom sensors that require excitation, but you must use a custom scale to scale the measured voltage.
							case DAQmx_Val_Voltage_CustomWithExcitation:
								InsertListItem(panHndl, controlID, -1, "Voltage with excitation", DAQmx_Val_Voltage_CustomWithExcitation);
								break;
								
							// Current measurement.
							case DAQmx_Val_Current:			
								InsertListItem(panHndl, controlID, -1, "Current", DAQmx_Val_Current);
								break;
								
							// Current RMS measurement.
							case DAQmx_Val_CurrentRMS:		
								InsertListItem(panHndl, controlID, -1, "Current RMS", DAQmx_Val_CurrentRMS); 
								break;
								
							// Temperature measurement using a thermocouple. 
							case DAQmx_Val_Temp_TC:
								InsertListItem(panHndl, controlID, -1, "Temperature with thermocouple", DAQmx_Val_Temp_TC);
								break;
								
							// Temperature measurement using an RTD.
							case DAQmx_Val_Temp_RTD:
								InsertListItem(panHndl, controlID, -1, "Temperature with RTD", DAQmx_Val_Temp_RTD);
								break;
								
							// Temperature measurement using a thermistor.
							case DAQmx_Val_Temp_Thrmstr:
								InsertListItem(panHndl, controlID, -1, "Temperature with thermistor", DAQmx_Val_Temp_Thrmstr);
								break;
							
							// Temperature measurement using a built-in sensor on a terminal block or device. On SCXI modules, for example, this could be the CJC sensor. 
							case DAQmx_Val_Temp_BuiltInSensor:
								InsertListItem(panHndl, controlID, -1, "Temperature with built in sensor", DAQmx_Val_Temp_BuiltInSensor); 
								break;
								
							// Frequency measurement using a frequency to voltage converter.
							case DAQmx_Val_Freq_Voltage:
								InsertListItem(panHndl, controlID, -1, "Frequency from voltage", DAQmx_Val_Freq_Voltage); 
								break;
								
							// Resistance measurement.
							case DAQmx_Val_Resistance:
								InsertListItem(panHndl, controlID, -1, "Resistance", DAQmx_Val_Resistance);
								break;
							
							// Strain measurement.
							case DAQmx_Val_Strain_Gage:
								InsertListItem(panHndl, controlID, -1, "Strain gage", DAQmx_Val_Strain_Gage);
								break;
								
							// Strain measurement using a rosette strain gage.
							case DAQmx_Val_Rosette_Strain_Gage :
								InsertListItem(panHndl, controlID, -1, "Strain with rosette strain gage", DAQmx_Val_Rosette_Strain_Gage); 
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
								
							// Torque measurement using a bridge-based sensor.
							case DAQmx_Val_Torque_Bridge:
								InsertListItem(panHndl, controlID, -1, "Torque using a bridge-based sensor", DAQmx_Val_Torque_Bridge); 
								break;
								
							// Measure voltage ratios from a Wheatstone bridge.
							case DAQmx_Val_Bridge:
								InsertListItem(panHndl, controlID, -1, "Wheatstone Bridge", DAQmx_Val_Bridge);
								break;
								
							// Acceleration measurement using an accelerometer.
							case DAQmx_Val_Accelerometer:
								InsertListItem(panHndl, controlID, -1, "Acceleration with an accelerometer", DAQmx_Val_Accelerometer);
								break;
								
							// Velocity measurement using an IEPE Sensor.
							case DAQmx_Val_Velocity_IEPESensor:
								InsertListItem(panHndl, controlID, -1, "Velocity with an IEPE sensor", DAQmx_Val_Velocity_IEPESensor); 
								break;
								
							// Sound pressure measurement using a microphone.
							case DAQmx_Val_SoundPressure_Microphone:
								InsertListItem(panHndl, controlID, -1, "Sound pressure using a microphone", DAQmx_Val_SoundPressure_Microphone);
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
								
							// Measurement type defined by TEDS.
							case DAQmx_Val_TEDS_Sensor:
								InsertListItem(panHndl, controlID, -1, "TEDS sensor", DAQmx_Val_TEDS_Sensor);
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
						InsertListItem(panHndl, controlID, -1, "Lines", Chan_DI_Line);
					
					if (ListNumItems(dev->attr->DIports))
						InsertListItem(panHndl, controlID, -1, "Ports", Chan_DI_Port);	
					
					break;
					
				case DAQmxCounter:
					
					n = ListNumItems(dev->attr->CISupportedMeasTypes);
					for (size_t i = 1; i <= n; i++) {
						measTypePtr = ListGetPtrToItem(dev->attr->CISupportedMeasTypes, i);
						switch (*measTypePtr) {
							
							// Measure the frequency of a digital signal.
							case DAQmx_Val_Freq: 		
								InsertListItem(panHndl, controlID, -1, "Frequency", DAQmx_Val_Freq); 
								break;
								
							// Measure the period of a digital signal.
							case DAQmx_Val_Period:			
								InsertListItem(panHndl, controlID, -1, "Period", DAQmx_Val_Period); 
								break;
								
							// Count edges of a digital signal.
							case DAQmx_Val_CountEdges:  		
								InsertListItem(panHndl, controlID, -1, "Edge count", DAQmx_Val_CountEdges); 
								break;
								
							// Measure the width of a pulse of a digital signal.
							case DAQmx_Val_PulseWidth:		
								InsertListItem(panHndl, controlID, -1, "Pulse width", DAQmx_Val_PulseWidth); 
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
								
							// Measure the time between state transitions of a digital signal.
							case DAQmx_Val_SemiPeriod:
								InsertListItem(panHndl, controlID, -1, "Semi period", DAQmx_Val_SemiPeriod); 
								break;
								
							// Measure time between edges of two digital signals.
							case DAQmx_Val_TwoEdgeSep:
								InsertListItem(panHndl, controlID, -1, "Two edge step", DAQmx_Val_TwoEdgeSep); 
								break;
								
							// Linear position measurement using a linear encoder.
							case DAQmx_Val_Position_LinEncoder:
								InsertListItem(panHndl, controlID, -1, "Position with linear encoder", DAQmx_Val_Position_LinEncoder); 
								break;
								
							// Angular position measurement using an angular encoder.
							case DAQmx_Val_Position_AngEncoder:
								InsertListItem(panHndl, controlID, -1, "Position with angular encoder", DAQmx_Val_Position_AngEncoder); 
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
						InsertListItem(panHndl, controlID, -1, "Lines", Chan_DO_Line);
					
					if (ListNumItems(dev->attr->DOports))
						InsertListItem(panHndl, controlID, -1, "Ports", Chan_DO_Port);	
					
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
	
	IORange->Nrngs 			= 0;
	IORange->selectedRange	= 0;
	IORange->low 			= NULL;
	IORange->high 			= NULL;
	
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
	
	for (int i = 0; i < IORngsPtr->Nrngs; i++){
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
	
	taskTrig->trigType					= Trig_None;
	taskTrig->device					= dev;
	taskTrig->trigSource				= NULL;
	taskTrig->slope						= TrigSlope_Rising;
	taskTrig->level						= 0;
	taskTrig->wndBttm					= 0;
	taskTrig->wndTop					= 0;
	taskTrig->wndTrigCond				= TrigWnd_Entering;
	taskTrig->nPreTrigSamples 			= 2;				// applies only to reference type trigger
	taskTrig->samplingRate				= samplingRate;
	
	// UI
	taskTrig->trigPanHndl				= 0;
	taskTrig->trigSlopeCtrlID			= 0;	
	taskTrig->trigSourceCtrlID			= 0;
	taskTrig->preTrigDurationCtrlID		= 0;
	taskTrig->preTrigNSamplesCtrlID		= 0;
	taskTrig->levelCtrlID				= 0;
	taskTrig->windowTrigCondCtrlID		= 0;
	taskTrig->windowTopCtrlID			= 0;
	taskTrig->windowBottomCtrlID		= 0;
	
		
	return taskTrig;	
}

static void	discard_TaskTrig_type (TaskTrig_type** taskTrigPtr)
{
	TaskTrig_type*	taskTrig = *taskTrigPtr;
	if (!taskTrig) return;
	
	OKfree(taskTrig->trigSource);
	
	OKfree(*taskTrigPtr);
}

//------------------------------------------------------------------------------ 
// ADTaskTiming_type
//------------------------------------------------------------------------------ 
static ADTaskTiming_type* init_ADTaskTiming_type (void)
{
	ADTaskTiming_type* taskTiming = malloc(sizeof(ADTaskTiming_type));
	if (!taskTiming) return NULL;
	
	taskTiming->measMode					= DAQmxDefault_Task_MeasMode;
	taskTiming->sampleRate					= DAQmxDefault_Task_SampleRate;
	taskTiming->targetSampleRate			= DAQmxDefault_Task_SampleRate;
	taskTiming->nSamples					= DAQmxDefault_Task_NSamples;
	taskTiming->oversampling				= 1;
	taskTiming->oversamplingAuto			= FALSE;
	taskTiming->blockSize					= DAQmxDefault_Task_BlockSize;
	taskTiming->sampClkSource				= NULL;   								// use onboard clock for sampling
	taskTiming->sampClkEdge					= SampClockEdge_Rising;
	taskTiming->refClkSource				= NULL;									// onboard clock has no external reference to sync to
	taskTiming->refClkFreq					= DAQmxDefault_Task_RefClkFreq;
	taskTiming->nSamplesSinkVChan			= NULL;
	taskTiming->nSamplesSourceVChan			= NULL;
	taskTiming->samplingRateSinkVChan		= NULL;
	taskTiming->samplingRateSourceVChan		= NULL;
	taskTiming->startSignalRouting			= NULL;
	
	// UI
	taskTiming->timingPanHndl				= 0;
	taskTiming->settingsPanHndl				= 0;
	
	return taskTiming;
}

static void discard_ADTaskTiming_type (ADTaskTiming_type** taskTimingPtr)
{
	ADTaskTiming_type*	taskTiming = *taskTimingPtr;
	if (!taskTiming) return;
	
	OKfree(taskTiming->sampClkSource);
	OKfree(taskTiming->refClkSource);
	OKfree(taskTiming->startSignalRouting);
	
	// VChans
	discard_VChan_type((VChan_type**)&taskTiming->nSamplesSinkVChan);
	discard_VChan_type((VChan_type**)&taskTiming->samplingRateSinkVChan);
	discard_VChan_type((VChan_type**)&taskTiming->nSamplesSourceVChan);
	discard_VChan_type((VChan_type**)&taskTiming->samplingRateSourceVChan);
	
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
	taskSet->dev				= dev;
	taskSet->chanSet			= 0;
	taskSet->timing				= NULL;
	taskSet->panHndl			= 0;
	taskSet->taskHndl			= NULL;
	taskSet->timeout			= DAQmxDefault_Task_Timeout;
	taskSet->startTrig			= NULL;
	taskSet->HWTrigMaster		= NULL;
	taskSet->HWTrigSlave		= NULL;
	taskSet->referenceTrig		= NULL;
	taskSet->writeAOData		= NULL;
	taskSet->readAIData			= NULL;
	taskSet->writeDOData		= NULL;
	taskSet->nOpenChannels		= 0;
	
	// allocate
	if (!(	taskSet->chanSet	= ListCreate(sizeof(ChanSet_type*)))) 	goto Error;
	if (!(	taskSet->timing		= init_ADTaskTiming_type()))			goto Error;	
			
		
	return taskSet;
	
Error:
	
	if (taskSet->chanSet) ListDispose(taskSet->chanSet);
	discard_ADTaskTiming_type(&taskSet->timing);
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
			(*(*chanSetPtr)->discardFptr)	((void**)chanSetPtr);
		}
		ListDispose((*taskSetPtr)->chanSet);
	}
	
	// trigger data
	discard_TaskTrig_type(&(*taskSetPtr)->startTrig);
	discard_TaskTrig_type(&(*taskSetPtr)->referenceTrig);
	discard_HWTrigMaster_type(&(*taskSetPtr)->HWTrigMaster);
	discard_HWTrigSlave_type(&(*taskSetPtr)->HWTrigSlave);
	
	// timing info
	discard_ADTaskTiming_type(&(*taskSetPtr)->timing);
	
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
INIT_ERR
	
	//--------------------------
	// load UI resources
	//--------------------------
	
	int ctrlIdx		= 0; 
	
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
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_SamplingRate, tskSet->timing->sampleRate/1000);					// display in [kHz]
	
	// set number of samples
	SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DATA_TYPE, VAL_UNSIGNED_64BIT_INTEGER);
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_NSamples, tskSet->timing->nSamples);
	
	// set block size
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_BlockSize, tskSet->timing->blockSize);
	
	// set duration
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate); 	// display in [s]
	
	// set oversampling properties if AI task
	if (tskSet->dev->AITaskSet == tskSet) {
		
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Oversampling, ATTR_VISIBLE, TRUE);
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_AutoOversampling, ATTR_VISIBLE, TRUE);
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_ActualSamplingRate, ATTR_VISIBLE, TRUE);
		
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_AutoOversampling, tskSet->timing->oversamplingAuto);			// set auto oversampling
		
		if (tskSet->timing->oversamplingAuto) {
			SetCtrlVal(tskSet->timing->settingsPanHndl, Set_TargetSamplingRate, tskSet->timing->targetSampleRate * 1e-3);	// set target sample rate in [kHz]
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_TargetSamplingRate, ATTR_VISIBLE, TRUE);
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Oversampling, ATTR_DIMMED, TRUE);
			// enforce oversampling
			tskSet->timing->oversampling = (uInt32)(tskSet->timing->targetSampleRate/tskSet->timing->sampleRate);
			if (!tskSet->timing->oversampling) tskSet->timing->oversampling = 1;
		} else {
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_TargetSamplingRate, ATTR_VISIBLE, FALSE);
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Oversampling, ATTR_DIMMED, FALSE);
		}
		
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Oversampling, tskSet->timing->oversampling);					// set oversampling factor
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_ActualSamplingRate, tskSet->timing->sampleRate * tskSet->timing->oversampling * 1e-3);	// display actual sampling rate in [kHz]
		
	} else {
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Oversampling, ATTR_VISIBLE, FALSE);
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_AutoOversampling, ATTR_VISIBLE, FALSE);
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_ActualSamplingRate, ATTR_VISIBLE, FALSE);
		SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_TargetSamplingRate, ATTR_VISIBLE, FALSE);
	}
	
	// set measurement mode
	InsertListItem(tskSet->timing->settingsPanHndl, Set_MeasMode, -1, "Finite", Operation_Finite);
	InsertListItem(tskSet->timing->settingsPanHndl, Set_MeasMode, -1, "Continuous", Operation_Continuous);
	  
	GetIndexFromValue(tskSet->timing->settingsPanHndl, Set_MeasMode, &ctrlIdx, tskSet->timing->measMode);
	SetCtrlIndex(tskSet->timing->settingsPanHndl, Set_MeasMode, ctrlIdx);
	
	// dim/undim NSamples and Duration depending on the measurement mode
	switch (tskSet->timing->measMode) {
		case Operation_Finite:
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Duration, ATTR_DIMMED, 0);
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DIMMED, 0); 
			break;
					
		case Operation_Continuous:
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Duration, ATTR_DIMMED, 1);
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DIMMED, 1); 
			break;
	}
	
	// add callback to controls in the panel
	SetCtrlsInPanCBInfo(tskSet, ADTaskSettings_CB, tskSet->timing->settingsPanHndl);
								
	//--------------------------
	// adjust "Timing" tab
	//--------------------------
	// get timing tab panel handle
	GetPanelHandleFromTabPage(tskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TimingTabIdx, &tskSet->timing->timingPanHndl);
	// add callback to controls in the panel
	// note: do this before changing the string controls to terminal controls!
	SetCtrlsInPanCBInfo(tskSet, ADTimingSettings_CB, tskSet->timing->timingPanHndl);
	// make sure that the host controls are not dimmed before inserting terminal controls!
	NIDAQmx_NewTerminalCtrl(tskSet->timing->timingPanHndl, Timing_SampleClkSource, 0); // single terminal selection
	NIDAQmx_NewTerminalCtrl(tskSet->timing->timingPanHndl, Timing_RefClkSource, 0);    // single terminal selection
								
	// adjust sample clock terminal control properties
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0); 
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_SampleClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
	// set default sample clock source if none was loaded
	if (!tskSet->timing->sampClkSource)
		tskSet->timing->sampClkSource = StrDup("OnboardClock");
	
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_SampleClkSource, tskSet->timing->sampClkSource);
					
	// adjust reference clock terminal control properties
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_Limit_To_Device, 0);
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_RefClkSource, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
					
	// set default reference clock source (none)
	if (!tskSet->timing->refClkSource)
		tskSet->timing->refClkSource = StrDup("");
		
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_RefClkSource, tskSet->timing->refClkSource);
					
	// set ref clock freq and timeout to default
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_RefClkFreq, tskSet->timing->refClkFreq / 1e6);		// display in [MHz]						
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_Timeout, tskSet->timeout);							// display in [s]
	
	// adjust signal routing control properties
	NIDAQmx_NewTerminalCtrl(tskSet->timing->timingPanHndl, Timing_StartRouting, 0); 					// single terminal selection
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_StartRouting, NIDAQmx_IOCtrl_Limit_To_Device, 0);
	NIDAQmx_SetTerminalCtrlAttribute(tskSet->timing->timingPanHndl, Timing_StartRouting, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
	
	if (!tskSet->timing->startSignalRouting)
		tskSet->timing->startSignalRouting = StrDup("");
	
	SetCtrlVal(tskSet->timing->timingPanHndl, Timing_StartRouting, tskSet->timing->startSignalRouting);
	
	//--------------------------
	// adjust "Trigger" tab
	//--------------------------
	// get trigger tab panel handle
	int trigPanHndl;
	GetPanelHandleFromTabPage(tskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TriggerTabIdx, &trigPanHndl);
	
	// remove "None" tab if there are supported triggers or triggers must be loaded
	if (taskTriggerUsage)
		DeleteTabPage(trigPanHndl, Trig_TrigSet, 0, 1);
	
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_Start) {
		// load resources
		int start_DigEdgeTrig_PanHndl = LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig1);
		// add start trigger panel
		newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Start");
		// get start trigger tab panel handle
		int startTrigPanHndl = 0;
		GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &startTrigPanHndl);
		// add control to select trigger type and put background plate
		DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_Plate, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		int trigTypeCtrlID = DuplicateCtrl(start_DigEdgeTrig_PanHndl, StartTrig1_TrigType, startTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		
		// add default "None" start trigger if none was loaded
		if (!tskSet->startTrig)
			tskSet->startTrig = init_TaskTrig_type(tskSet->dev, &tskSet->timing->sampleRate);
		
		// add panel info
		tskSet->startTrig->trigPanHndl = startTrigPanHndl;
			
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
		
		// select trigger type
		GetIndexFromValue(startTrigPanHndl, trigTypeCtrlID, &ctrlIdx, tskSet->startTrig->trigType);
		SetCtrlIndex(startTrigPanHndl, trigTypeCtrlID, ctrlIdx); 
		
		// update trigger UI
		AddStartTriggerToUI(tskSet->startTrig);
		
		// add callback data and callback function to the control
		SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, tskSet->startTrig);
		SetCtrlAttribute(startTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskStartTrigType_CB);
		
		// discard resources
		DiscardPanel(start_DigEdgeTrig_PanHndl);
	}
	
	if (taskTriggerUsage & DAQmx_Val_Bit_TriggerUsageTypes_Reference) {
		// load resources
		int reference_DigEdgeTrig_PanHndl	= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig1);
		// add reference trigger panel
		newTabIdx = InsertTabPage(trigPanHndl, Trig_TrigSet, -1, "Reference");
		// get reference trigger tab panel handle
		int referenceTrigPanHndl;
		GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, newTabIdx, &referenceTrigPanHndl);
		// add control to select trigger type
		DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_Plate, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		int trigTypeCtrlID = DuplicateCtrl(reference_DigEdgeTrig_PanHndl, RefTrig1_TrigType, referenceTrigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);
		
		// add default "None" reference trigger if none was loaded
		if (!tskSet->referenceTrig)
			tskSet->referenceTrig = init_TaskTrig_type(tskSet->dev, &tskSet->timing->sampleRate);
		
		// add panel info
		tskSet->referenceTrig->trigPanHndl = referenceTrigPanHndl;
		
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
		
		// select trigger type
		GetIndexFromValue(referenceTrigPanHndl, trigTypeCtrlID, &ctrlIdx, tskSet->referenceTrig->trigType);
		SetCtrlIndex(referenceTrigPanHndl, trigTypeCtrlID, ctrlIdx); 
		
		// update trigger UI
		AddReferenceTriggerToUI(tskSet->referenceTrig);
								
		// add callback data and callback function to the control
		SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_DATA, tskSet->referenceTrig);
		SetCtrlAttribute(referenceTrigPanHndl, trigTypeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TaskReferenceTrigType_CB);
		
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
						
	DLDataTypes		nSamplesVChanAllowedDataTypes[] = {DL_UChar, DL_UShort, DL_UInt, DL_UInt64};
						
	tskSet->timing->nSamplesSinkVChan	= init_SinkVChan_type(nSamplesSinkVChanName, nSamplesVChanAllowedDataTypes, NumElem(nSamplesVChanAllowedDataTypes),
															tskSet, VChan_Data_Receive_Timeout, ADNSamplesSinkVChan_StateChange);
	OKfree(nSamplesSinkVChanName);
	// register VChan with the framework
	DLRegisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->nSamplesSinkVChan);
	// register VChan with the DAQmx task controller
	errChk( AddSinkVChan(tskSet->dev->taskController, tskSet->timing->nSamplesSinkVChan, ADNSamples_DataReceivedTC, NULL) );
						
	//-------------------------------------------------------------------------
	// add nSamples Source VChan to send number of required samples
	//-------------------------------------------------------------------------
						
	// create VChan
	char*	nSamplesSourceVChanName = GetTaskControlName(tskSet->dev->taskController);
	AppendString(&nSamplesSourceVChanName, ": ", -1);
	AppendString(&nSamplesSourceVChanName, sourceVChanNSamplesBaseName, -1);
						
	tskSet->timing->nSamplesSourceVChan	= init_SourceVChan_type(nSamplesSourceVChanName, DL_UInt64, tskSet, ADNSamplesSourceVChan_StateChange);
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
																	tskSet, VChan_Data_Receive_Timeout, ADSamplingRateSinkVChan_StateChange);
	OKfree(samplingRateSinkVChanName);
	// register VChan with the framework
	DLRegisterVChan((DAQLabModule_type*)tskSet->dev->niDAQModule, (VChan_type*)tskSet->timing->samplingRateSinkVChan);
	// register VChan with the DAQmx task controller
	errChk( AddSinkVChan(tskSet->dev->taskController, tskSet->timing->samplingRateSinkVChan, ADSamplingRate_DataReceivedTC, NULL) );
						
	//-------------------------------------------------------------------------
	// add samplingRate Source VChan to pick-up sampling rate
	//-------------------------------------------------------------------------
						
	// create VChan
	char*	samplingRateSourceVChanName = GetTaskControlName(tskSet->dev->taskController);
	AppendString(&samplingRateSourceVChanName, ": ", -1);
	AppendString(&samplingRateSourceVChanName, sourceVChanSamplingRateBaseName, -1);
						
	tskSet->timing->samplingRateSourceVChan	= init_SourceVChan_type(samplingRateSourceVChanName, DL_Double, tskSet, ADSamplingRateSourceVChan_StateChange);
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
	
Error:
	
	return;
}

static void	discardUI_ADTaskSet (ADTaskSet_type** taskSetPtr)
{
	ADTaskSet_type* taskSet = *taskSetPtr;
	//------------------------------------------
	// remove nSamples Sink & Source VChans
	//------------------------------------------
				
	// remove from framework
	DLUnregisterVChan((DAQLabModule_type*)taskSet->dev->niDAQModule, (VChan_type*)taskSet->timing->nSamplesSinkVChan);
	DLUnregisterVChan((DAQLabModule_type*)taskSet->dev->niDAQModule, (VChan_type*)taskSet->timing->nSamplesSourceVChan);
	// detach from Task Controller										 
	RemoveSinkVChan(taskSet->dev->taskController, taskSet->timing->nSamplesSinkVChan, NULL);
				
	//------------------------------------------
	// remove Sink & Source sampling rate VChan
	//------------------------------------------
				
	// remove from framework
	DLUnregisterVChan((DAQLabModule_type*)taskSet->dev->niDAQModule, (VChan_type*)taskSet->timing->samplingRateSinkVChan);
	DLUnregisterVChan((DAQLabModule_type*)taskSet->dev->niDAQModule, (VChan_type*)taskSet->timing->samplingRateSourceVChan);
	// detach from Task Controller										 
	RemoveSinkVChan(taskSet->dev->taskController, taskSet->timing->samplingRateSinkVChan, NULL);
				
	//------------------------------------------
	// remove HW triggers from framework
	//------------------------------------------
				
	DLUnregisterHWTrigMaster(taskSet->HWTrigMaster);
	DLUnregisterHWTrigSlave(taskSet->HWTrigSlave);
			
	//------------------------------------------ 
	// discard AIAO task settings and VChan data
	//------------------------------------------
				
	discard_ADTaskSet_type(taskSetPtr);
}

// UI callback to adjust AI task settings
static int ADTaskSettings_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR
	
	ADTaskSet_type*		tskSet 				= callbackData;
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
			
			// if oversampling must be adjusted automatically
			if (tskSet->timing->oversamplingAuto) {
				tskSet->timing->oversampling = (uInt32)(tskSet->timing->targetSampleRate/tskSet->timing->sampleRate);
				if (!tskSet->timing->oversampling) tskSet->timing->oversampling = 1;
				SetCtrlVal(panel, Set_Oversampling, tskSet->timing->oversampling);
				
				// update also read AI data structures
				discard_ReadAIData_type(&tskSet->readAIData);
				tskSet->readAIData = init_ReadAIData_type(tskSet->dev);
			}
			
			// given oversampling, adjust also the actual DAQ sampling rate display
			SetCtrlVal(panel, Set_ActualSamplingRate, tskSet->timing->sampleRate * tskSet->timing->oversampling * 1e-3);	// display in [kHz]
			
			// send sampling rate
			nullChk( samplingRatePtr = malloc(sizeof(double)) );
			*samplingRatePtr = tskSet->timing->sampleRate;
			nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**)&samplingRatePtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
			
			break;
			
		case Set_NSamples:
			
			GetCtrlVal(panel, control, &tskSet->timing->nSamples);
			// adjust duration display
			SetCtrlVal(panel, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate);
			
			// send number of samples
			nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
			*nSamplesPtr = tskSet->timing->nSamples;
			nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
			
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
			nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**)&nSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
			
			break;
			
		case Set_Oversampling:
			
			GetCtrlVal(panel, control, &tskSet->timing->oversampling);
			// given final sampling rate, adjust also the actual DAQ sampling rate display
			SetCtrlVal(panel, Set_ActualSamplingRate, tskSet->timing->sampleRate * tskSet->timing->oversampling * 1e-3);	// display in [kHz]
			
			discard_ReadAIData_type(&tskSet->readAIData);
			tskSet->readAIData = init_ReadAIData_type(tskSet->dev);
			break;
			
		case Set_AutoOversampling:
			
			GetCtrlVal(panel, control, &tskSet->timing->oversamplingAuto);
			
			if (tskSet->timing->oversamplingAuto) {
				SetCtrlAttribute(panel, Set_Oversampling, ATTR_DIMMED, TRUE);
				// set target sampling rate display to be equal to the product between sampling rate and oversampling
				tskSet->timing->targetSampleRate = tskSet->timing->sampleRate * tskSet->timing->oversampling;
				SetCtrlVal(panel, Set_TargetSamplingRate, tskSet->timing->targetSampleRate * 1e-3);		// display in [kHz]
				SetCtrlAttribute(panel, Set_TargetSamplingRate, ATTR_VISIBLE, TRUE);
			} else {
				SetCtrlAttribute(panel, Set_Oversampling, ATTR_DIMMED, FALSE);
				SetCtrlAttribute(panel, Set_TargetSamplingRate, ATTR_VISIBLE, FALSE);
			}
			
			break;
			
		case Set_TargetSamplingRate:
			
			GetCtrlVal(panel, control, &tskSet->timing->targetSampleRate);	// read in [kHz]
			tskSet->timing->targetSampleRate *= 1e3;						// convert to [Hz]
			
			// calculate oversampling
			tskSet->timing->oversampling = (uInt32)(tskSet->timing->targetSampleRate/tskSet->timing->sampleRate);
			if (!tskSet->timing->oversampling) tskSet->timing->oversampling = 1;
			SetCtrlVal(panel, Set_Oversampling, tskSet->timing->oversampling);
			
			// update read AI data structures
			discard_ReadAIData_type(&tskSet->readAIData);
			tskSet->readAIData = init_ReadAIData_type(tskSet->dev);
			
			// update actual DAQ sampling rate display
			SetCtrlVal(panel, Set_ActualSamplingRate, tskSet->timing->oversampling * tskSet->timing->sampleRate * 1e-3); 	// display in [kHz]
			break;
			
		case Set_BlockSize:
			
			GetCtrlVal(panel, control, &tskSet->timing->blockSize);
			break;
			
		case Set_MeasMode:
			
			unsigned int measMode;
			GetCtrlVal(panel, control, &measMode);
			tskSet->timing->measMode = measMode;
			
			switch (tskSet->timing->measMode) {
				case Operation_Finite:
					SetCtrlAttribute(panel, Set_Duration, ATTR_DIMMED, 0);
					SetCtrlAttribute(panel, Set_NSamples, ATTR_DIMMED, 0); 
					break;
					
				case Operation_Continuous:
					SetCtrlAttribute(panel, Set_Duration, ATTR_DIMMED, 1);
					SetCtrlAttribute(panel, Set_NSamples, ATTR_DIMMED, 1); 
					break;
			}
			break;
	}
	
	// update device settings
	if (tskSet->dev->AITaskSet == tskSet)
		errChk ( ConfigDAQmxAITask(tskSet->dev, &errorInfo.errMsg) );
	
	if (tskSet->dev->AOTaskSet == tskSet)
		errChk ( ConfigDAQmxAOTask(tskSet->dev, &errorInfo.errMsg) );
	
	if (tskSet->dev->DITaskSet == tskSet)
		errChk ( ConfigDAQmxDITask(tskSet->dev, &errorInfo.errMsg) );
	
	if (tskSet->dev->DOTaskSet == tskSet)
		errChk ( ConfigDAQmxDOTask(tskSet->dev, &errorInfo.errMsg) );
	
	return 0;
	
Error:
	
	// cleanup
	OKfree(nSamplesPtr);
	OKfree(samplingRatePtr);
	discard_DataPacket_type(&dataPacket); 
	
PRINT_ERR
	
	return 0;
}

static int ADTimingSettings_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ADTaskSet_type*		tskSet 			= callbackData;  
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case Timing_SampleClkSource:
			{	
				int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(tskSet->timing->sampClkSource);
				tskSet->timing->sampClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, tskSet->timing->sampClkSource);
			}
			break;
			
		case Timing_RefClkSource:
			{	
				int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(tskSet->timing->refClkSource);
				tskSet->timing->refClkSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, tskSet->timing->refClkSource);
			}
			break;
			
		case Timing_StartRouting:
			{	
				int buffsize = 0;
				GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
				OKfree(tskSet->timing->startSignalRouting);
				tskSet->timing->startSignalRouting = malloc((buffsize+1) * sizeof(char)); // including ASCII null
				GetCtrlVal(panel, control, tskSet->timing->startSignalRouting);
			}
			break;
			
		case Timing_RefClkFreq:
			
			GetCtrlVal(panel, control, &tskSet->timing->refClkFreq);  				// read in [MHz]
			tskSet->timing->refClkFreq *= 1e6;										// convert to [Hz]
			break;
			
		case Timing_Timeout:
			
			GetCtrlVal(panel, control, &tskSet->timeout);							// read in [s]
			break;
			
	}
	
	// update device settings
	if (tskSet->dev->AITaskSet == tskSet)
		errChk ( ConfigDAQmxAITask(tskSet->dev, &errorInfo.errMsg) );
	
	if (tskSet->dev->AOTaskSet == tskSet)
		errChk ( ConfigDAQmxAOTask(tskSet->dev, &errorInfo.errMsg) );
	
	if (tskSet->dev->DITaskSet == tskSet)
		errChk ( ConfigDAQmxDITask(tskSet->dev, &errorInfo.errMsg) );
	
	if (tskSet->dev->DOTaskSet == tskSet)
		errChk ( ConfigDAQmxDOTask(tskSet->dev, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return 0;
}

//------------------------------------------------------------------------------
// ReadAIData_type
//------------------------------------------------------------------------------
static ReadAIData_type* init_ReadAIData_type (Dev_type* dev)
{
	ReadAIData_type*	readAI 		= NULL;
	size_t				nAITotal	= ListNumItems(dev->AITaskSet->chanSet);
	ChanSet_type*		chanSet		= NULL;
	size_t				nAI			= 0; 
	size_t				chIdx		= 0;
	
	// allocate memory for processing AI data
	readAI 		= malloc (sizeof(ReadAIData_type));
	if (!readAI) return NULL;
	
	// count number of AI channels using HW-timing
	for (size_t i = 1; i <= nAITotal; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
		nAI++;
	}
	
	// init
	readAI->nAI 			= nAI;
	readAI->nIntBuffElem 	= NULL;
	readAI->intBuffers 		= NULL;
	
	if (!nAI) return readAI;
	
	// alloc
	if ( !(readAI->nIntBuffElem = calloc(nAI, sizeof(uInt32))) ) goto Error;
	if ( !(readAI->intBuffers 	= calloc(nAI, sizeof(float64*))) ) goto Error;
	for (size_t i = 1; i <= nAITotal; i++) {
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
		
		if ( !(readAI->intBuffers[chIdx] = calloc(dev->AITaskSet->timing->oversampling * dev->AITaskSet->timing->blockSize, sizeof(float64))) ) goto Error;
		
		chIdx++;
	}
	
	return readAI;
	
Error:
	
	discard_ReadAIData_type(&readAI);
	return NULL;
}

static void discard_ReadAIData_type (ReadAIData_type** readAIPtr)
{
	ReadAIData_type*	readAI = *readAIPtr;
	
	if (!readAI) return;
	
	OKfree(readAI->nIntBuffElem);
	
	if (readAI->intBuffers) {
		for (size_t i = 0; i < readAI->nAI; i++)
			OKfree(readAI->intBuffers[i]);
		
		OKfree(readAI->intBuffers);
	}
	
	OKfree(*readAIPtr);
}

//------------------------------------------------------------------------------
// WriteAOData_type
//------------------------------------------------------------------------------
static WriteAOData_type* init_WriteAOData_type (Dev_type* dev)
{
	size_t				nChans		= ListNumItems(dev->AOTaskSet->chanSet); 
	ChanSet_type*		chanSet		= NULL;
	size_t				nAO			= 0;
	WriteAOData_type* 	writeData	= malloc(sizeof(WriteAOData_type));
	
	if (!writeData) return NULL;
	
	// count number of open AO channels using HW-timing
	for (size_t i = 1; i <= nChans; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->sinkVChan)) nAO++;
	}
	
	// init
	writeData->writeblock					= dev->AOTaskSet->timing->blockSize;
	writeData->numchan						= nAO;
	writeData->datain           			= NULL;
	writeData->databuff         			= NULL;
	writeData->dataout          			= NULL;
	writeData->sinkVChans        			= NULL;
	writeData->datain_size      			= NULL;
	writeData->databuff_size    			= NULL;
	writeData->idx              			= NULL;
	writeData->datain_repeat    			= NULL;
	writeData->datain_remainder 			= NULL;
	writeData->datain_loop      			= NULL;
	writeData->nullPacketReceived			= NULL;
	writeData->writeBlocksLeftToWrite		= 0;
	
	// datain
	if (!(	writeData->datain				= malloc(nAO * sizeof(float64*))) && nAO)							goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->datain[i] = NULL;
	
	// databuff
	if (!(	writeData->databuff   			= malloc(nAO * sizeof(float64*))) && nAO)							goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->databuff[i] = NULL;
	
	// dataout
	if (!(	writeData->dataout 				= malloc(nAO * writeData->writeblock * sizeof(float64))) && nAO)	goto Error;
	
	// sink VChans
	if (!(	writeData->sinkVChans			= malloc(nAO * sizeof(SinkVChan_type*))) && nAO)					goto Error;
	
	size_t k = 0;
	for (size_t i = 1; i <= nChans; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->sinkVChan)) {
			writeData->sinkVChans[k] = chanSet->sinkVChan;
			k++;
		}
	}
	
	// datain_size
	if (!(	writeData->datain_size 			= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->datain_size[i] = 0;
		
	// databuff_size
	if (!(	writeData->databuff_size 		= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->databuff_size[i] = 0;
		
	// idx
	if (!(	writeData->idx 					= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->idx[i] = 0;
		
	// datain_repeat
	if (!(	writeData->datain_repeat 		= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->datain_repeat[i] = 0;
		
	// datain_remainder
	if (!(	writeData->datain_remainder 	= malloc(nAO * sizeof(size_t))) && nAO)								goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->datain_remainder[i] = 0;
		
	// datain_loop
	if (!(	writeData->datain_loop 			= malloc(nAO * sizeof(BOOL))) && nAO)								goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->datain_loop[i] = FALSE;
	
	// nullPacketReceived
	if (!(	writeData->nullPacketReceived	= malloc(nAO * sizeof(BOOL))) && nAO)								goto Error;
	for (size_t i = 0; i < nAO; i++) writeData->nullPacketReceived[i] = FALSE;
	
	return writeData;
	
Error:
	
	discard_WriteAOData_type(&writeData);
	
	return NULL;
}

static void	discard_WriteAOData_type (WriteAOData_type** writeDataPtr)
{
	WriteAOData_type*	writeData = *writeDataPtr;
	
	if (!writeData) return;
	
	for (size_t i = 0; i < writeData->numchan; i++) {
		OKfree(writeData->datain[i]);	    
		OKfree(writeData->databuff[i]);   
	}
	OKfree(writeData->databuff);
	OKfree(writeData->datain); 
	
	OKfree(writeData->dataout);
	OKfree(writeData->sinkVChans);
	OKfree(writeData->datain_size);
	OKfree(writeData->databuff_size);
	OKfree(writeData->idx);
	OKfree(writeData->datain_repeat);
	OKfree(writeData->datain_remainder);
	OKfree(writeData->datain_loop);
	OKfree(writeData->nullPacketReceived);
	
	OKfree(*writeDataPtr);
}

//------------------------------------------------------------------------------
// WriteDOData_type
//------------------------------------------------------------------------------
static WriteDOData_type* init_WriteDOData_type (Dev_type* dev)
{
	size_t				nChans		= ListNumItems(dev->DOTaskSet->chanSet); 
	ChanSet_type*		chanSet		= NULL;
	size_t				nDO			= 0;
	WriteDOData_type* 	writeData	= malloc(sizeof(WriteDOData_type));
	
	if (!writeData) return NULL;
	
	// count number of DO channels using HW-timing
	for (size_t i = 1; i <= nChans; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->sinkVChan)) nDO++;
	}
	
	// init
	writeData->writeblock					= dev->DOTaskSet->timing->blockSize;
	writeData->numchan						= nDO;
	writeData->datain           			= NULL;
	writeData->databuff         			= NULL;
	writeData->dataout          			= NULL;
	writeData->sinkVChans        			= NULL;
	writeData->datain_size      			= NULL;
	writeData->databuff_size    			= NULL;
	writeData->idx              			= NULL;
	writeData->datain_repeat    			= NULL;
	writeData->datain_remainder 			= NULL;
	writeData->datain_loop      			= NULL;
	writeData->nullPacketReceived			= NULL;
	writeData->writeBlocksLeftToWrite		= 0;
	
	// datain
	if (!(	writeData->datain				= malloc(nDO * sizeof(uInt32*))) && nDO) 							goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->datain[i] = NULL;
	
	// databuff
	if (!(	writeData->databuff   			= malloc(nDO * sizeof(uInt32*))) && nDO) 							goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->databuff[i] = NULL;
	
	// dataout
	if (!(	writeData->dataout 				= malloc(nDO * writeData->writeblock * sizeof(uInt32))) && nDO)		goto Error;
	
	// sink VChans
	if (!(	writeData->sinkVChans			= malloc(nDO * sizeof(SinkVChan_type*))) && nDO)					goto Error;
	
	size_t k = 0;
	for (size_t i = 1; i <= nChans; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->sinkVChan)) {
			writeData->sinkVChans[k] = chanSet->sinkVChan;
			k++;
		}
	}
	
	// datain_size
	if (!(	writeData->datain_size 			= malloc(nDO * sizeof(size_t))) && nDO)								goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->datain_size[i] = 0;
		
	// databuff_size
	if (!(	writeData->databuff_size 		= malloc(nDO * sizeof(size_t))) && nDO)								goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->databuff_size[i] = 0;
		
	// idx
	if (!(	writeData->idx 					= malloc(nDO * sizeof(size_t))) && nDO)								goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->idx[i] = 0;
		
	// datain_repeat
	if (!(	writeData->datain_repeat 		= malloc(nDO * sizeof(size_t))) && nDO)								goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->datain_repeat[i] = 0;
		
	// datain_remainder
	if (!(	writeData->datain_remainder 	= malloc(nDO * sizeof(size_t))) && nDO)								goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->datain_remainder[i] = 0;
		
	// datain_loop
	if (!(	writeData->datain_loop 			= malloc(nDO * sizeof(BOOL))) && nDO)								goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->datain_loop[i] = FALSE;
	
	// nullPacketReceived
	if (!(	writeData->nullPacketReceived	= malloc(nDO * sizeof(BOOL))) && nDO)								goto Error;
	for (size_t i = 0; i < nDO; i++) writeData->nullPacketReceived[i] = FALSE;
	
	return writeData;
	
Error:
	
	discard_WriteDOData_type(&writeData);
	
	return NULL;
}

static void	discard_WriteDOData_type (WriteDOData_type** writeDataPtr)
{
	WriteDOData_type*	writeData = *writeDataPtr;
	
	if (!writeData) return;
	
	for (size_t i = 0; i < writeData->numchan; i++) {
		OKfree(writeData->datain[i]);
		OKfree(writeData->databuff[i]);
	}
	OKfree(writeData->databuff);
	OKfree(writeData->datain); 
	
	OKfree(writeData->dataout);
	OKfree(writeData->sinkVChans);
	OKfree(writeData->datain_size);
	OKfree(writeData->databuff_size);
	OKfree(writeData->idx);
	OKfree(writeData->datain_repeat);
	OKfree(writeData->datain_remainder);
	OKfree(writeData->datain_loop);
	OKfree(writeData->nullPacketReceived);
	
	OKfree(*writeDataPtr);
}

static int DO_Timing_TaskSet_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	Dev_type*			dev 			= callbackData;
	
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
	errChk ( ConfigDAQmxDOTask(dev, &errorInfo.errMsg) );  
	
Error:
	
PRINT_ERR

	return 0;
}

//------------------------------------------------------------------------------
// CITaskSet_type
//------------------------------------------------------------------------------
static CITaskSet_type* init_CITaskSet_type (Dev_type* dev)
{
	CITaskSet_type* taskSet = malloc (sizeof(CITaskSet_type));
	if (!taskSet) return NULL;
	
	// init
	taskSet->timeout				= DAQmxDefault_Task_Timeout;
	taskSet->chanTaskSet			= 0;
	taskSet->dev					= dev;
	taskSet->panHndl				= 0;
	taskSet->taskHndl				= 0;
	
	if (!(	taskSet->chanTaskSet	= ListCreate(sizeof(ChanSet_type*)))) 	goto Error;
	
	return taskSet;
	
Error:
	
	if (taskSet->chanTaskSet) ListDispose(taskSet->chanTaskSet);
	OKfree(taskSet);
	return NULL;
}

static void	discard_CITaskSet_type (CITaskSet_type** taskSetPtr)
{
	CITaskSet_type* taskSet = *taskSetPtr;
	if (!taskSet) return;
	
	// channels and DAQmx tasks
	if (taskSet->chanTaskSet) {
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems(taskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem(taskSet->chanTaskSet, i);
			(*(*chanSetPtr)->discardFptr)	((void**)chanSetPtr);
		}
		ListDispose(taskSet->chanTaskSet);
	}
	
	OKfree(taskSetPtr);
}

//------------------------------------------------------------------------------
// COTaskSet_type
//------------------------------------------------------------------------------
static COTaskSet_type* init_COTaskSet_type (Dev_type* dev)
{
	COTaskSet_type* taskSet = malloc (sizeof(COTaskSet_type));
	if (!taskSet) return NULL;
	
	// init
	//taskSet->timeout				= DAQmxDefault_Task_Timeout;
	taskSet->chanTaskSet			= 0;
	taskSet->dev					= dev;
	taskSet->panHndl				= 0;
	taskSet->taskHndl				= 0;
	
	if (!(	taskSet->chanTaskSet	= ListCreate(sizeof(ChanSet_CO_type*)))) 	goto Error;
	
	return taskSet;
	
Error:
	
	if (taskSet->chanTaskSet) ListDispose(taskSet->chanTaskSet);
	OKfree(taskSet);
	return NULL;
}

static void discard_COTaskSet_type (COTaskSet_type** taskSetPtr)
{
	COTaskSet_type*	taskSet = *taskSetPtr;
	if (!taskSet) return;
	
		// channels and DAQmx tasks
	if (taskSet->chanTaskSet) {
		ChanSet_type** 	chanSetPtr;
		size_t			nChans			= ListNumItems(taskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {	
			chanSetPtr = ListGetPtrToItem(taskSet->chanTaskSet, i);
			(*(*chanSetPtr)->discardFptr)	((void**)chanSetPtr);
		}
		ListDispose(taskSet->chanTaskSet);
	}
	
	OKfree(*taskSetPtr);
}

static int CVICALLBACK Chan_CO_Pulse_Frequency_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR	
	
	ChanSet_CO_type* 	selectedChan 	= callbackData;
	DataPacket_type*  	dataPacket		= NULL;
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
			if (generationMode == Operation_Finite) {
				SetPulseTrainMode(selectedChan->pulseTrain, Operation_Finite);
				// undim N Pulses
				SetCtrlAttribute(panel, COFreqPan_NPulses, ATTR_DIMMED, 0); 
			} else {
				SetPulseTrainMode(selectedChan->pulseTrain, Operation_Continuous);
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
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errorInfo.errMsg) );
	
	// send new pulse train
	nullChk( pulseTrain	= CopyPulseTrain(selectedChan->pulseTrain) ); 
	nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) );
	errChk( SendDataPacket(selectedChan->baseClass.srcVChan, &dataPacket, 0, &errorInfo.errMsg) ); 
	
	
	return 0;
	
Error:
	
	// clean up
	discard_DataPacket_type(&dataPacket);
	discard_PulseTrain_type(&pulseTrain);
	
PRINT_ERR
	
	return 0;
}

static int CVICALLBACK Chan_CO_Pulse_Time_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ChanSet_CO_type* 	selectedChan 	= callbackData;
	DataPacket_type*  	dataPacket		= NULL;
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
			if (generationMode == Operation_Finite) {
				SetPulseTrainMode(selectedChan->pulseTrain, Operation_Finite);
				// undim N Pulses
				SetCtrlAttribute(panel, COTimePan_NPulses, ATTR_DIMMED, 0); 
			} else {
				SetPulseTrainMode(selectedChan->pulseTrain, Operation_Continuous);
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
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errorInfo.errMsg) );
	
	// send new pulse train
	nullChk( pulseTrain	= CopyPulseTrain(selectedChan->pulseTrain) ); 
	nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) );
	errChk( SendDataPacket(selectedChan->baseClass.srcVChan, &dataPacket, 0, &errorInfo.errMsg) ); 
	
	return 0;
	
Error:
	
	// clean up
	discard_DataPacket_type(&dataPacket);
	discard_PulseTrain_type(&pulseTrain);
	
PRINT_ERR	
	
	return 0;
}

static int CVICALLBACK Chan_CO_Pulse_Ticks_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ChanSet_CO_type* 	selectedChan 	= callbackData;
	DataPacket_type*  	dataPacket		= NULL;
	int					generationMode	= 0;
	int					ticks			= 0;
	int 				idleState		= 0;
	uInt64				nPulses			= 0;
	PulseTrain_type* 	pulseTrain		= NULL;
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) {
			
		case COTicksPan_Mode:
			GetCtrlVal(panel, control, &generationMode);
			if (generationMode == Operation_Finite) {
				SetPulseTrainMode(selectedChan->pulseTrain, Operation_Finite);
				// undim N Pulses
				SetCtrlAttribute(panel, COTicksPan_NPulses, ATTR_DIMMED, 0); 
			} else {
				SetPulseTrainMode(selectedChan->pulseTrain, Operation_Continuous); 
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
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errorInfo.errMsg) );
	
	// send new pulse train
	nullChk( pulseTrain	= CopyPulseTrain(selectedChan->pulseTrain) ); 
	nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) );
	errChk( SendDataPacket(selectedChan->baseClass.srcVChan, &dataPacket, 0, &errorInfo.errMsg) ); 
	
	return 0;
	
Error:
	
	// clean up
	discard_DataPacket_type(&dataPacket);
	discard_PulseTrain_type(&pulseTrain);
	
PRINT_ERR	
	
	return 0;
}

static int CVICALLBACK Chan_CO_Clk_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ChanSet_CO_type* 	selectedChan 	= callbackData; 
	int 				buffSize 		= 0; 
	
	if (event != EVENT_COMMIT) return 0;
	
	switch (control) { 
			
		case TimingPan_ClockSource:
			
			OKfree(selectedChan->clockSource);
			GetCtrlValStringLength(panel, control, &buffSize);
			selectedChan->clockSource = malloc((buffSize+1) * sizeof(char)); // including ASCII null
			GetCtrlVal(panel, control, selectedChan->clockSource);
			break;
			
	//	case TimingPan_Timeout:
			
	//		GetCtrlVal(panel, control, &selectedChan->baseClass.device->COTaskSet->timeout);
	//		break;
	}
	
	// configure CO task
	errChk(ConfigDAQmxCOTask(selectedChan->baseClass.device, &errorInfo.errMsg) );
	
	return 0;
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK Chan_CO_Trig_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	ChanSet_CO_type* 			selectedChan 		= callbackData; 
	int 						buffsize 			= 0; 
	int 						intval;
	
	
	if (event != EVENT_COMMIT) return 0;
	
	
	switch (control) { 							  
			
		case TrigPan_Slope:
			
		  	GetCtrlVal(panel, control, &intval);
			if (intval == TrigSlope_Rising) selectedChan->startTrig->slope = TrigSlope_Rising;
			if (intval == TrigSlope_Falling) selectedChan->startTrig->slope = TrigSlope_Falling; 
			break;
			
 		case TrigPan_TrigType:
			
			GetCtrlVal(panel, control,&intval); 
			if (intval == Trig_None) {
				selectedChan->startTrig->trigType = DAQmx_Val_None;
				SetCtrlAttribute(panel,TrigPan_Slope,ATTR_DIMMED,TRUE);
				SetCtrlAttribute(panel,TrigPan_Source,ATTR_DIMMED,TRUE); 
			}
			
			if (intval == Trig_DigitalEdge) {
				selectedChan->startTrig->trigType = DAQmx_Val_DigEdge;
				SetCtrlAttribute(panel,TrigPan_Slope,ATTR_DIMMED,FALSE);
				SetCtrlAttribute(panel,TrigPan_Source,ATTR_DIMMED,FALSE); 
			}
			break;
			
		case TrigPan_Source:
			
			OKfree(selectedChan->startTrig->trigSource);
			GetCtrlValStringLength(panel, control, &buffsize);
			selectedChan->startTrig->trigSource = malloc((buffsize+1) * sizeof(char)); // including ASCII null
			GetCtrlVal(panel, TrigPan_Source, selectedChan->startTrig->trigSource);
			break;
			
	}
	
	// configure CO task
	errChk( ConfigDAQmxCOTask(selectedChan->baseClass.device, &errorInfo.errMsg) );
	
	return 0;
	
Error:
	
PRINT_ERR	
	
	return 0;
	
}

static int DAQmxDevTaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	Dev_type* 		dev	 		= (Dev_type*) callbackData;
	int				ioVal		= 0;
	int				ioMode		= 0;
	int				ioType		= 0;
	
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
					
					errChk( TaskControlEvent(dev->taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
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
					errChk( AddDAQmxChannel(dev, ioVal, ioMode, ioType, chanName, &errorInfo.errMsg) );
					
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
	
Error:
	
PRINT_ERR	
	
	return 0;
}

//---------------------------------------------------------------------------------------------------------------
// DAQmx Device Task Configuration
//---------------------------------------------------------------------------------------------------------------

static int ConfigDAQmxDevice (Dev_type* dev, char** errorMsg)
{
INIT_ERR	
	
	// Configure AI DAQmx Task
	errChk( ConfigDAQmxAITask(dev, &errorInfo.errMsg) );
	
	// Configure AO DAQmx Task
	errChk( ConfigDAQmxAOTask(dev, &errorInfo.errMsg) );
	
	// Configure DI DAQmx Task
	errChk( ConfigDAQmxDITask(dev, &errorInfo.errMsg) );
	
	// Configure DO DAQmx Task
	errChk( ConfigDAQmxDOTask(dev, &errorInfo.errMsg) );
	
	// Configure CI DAQmx Task
	errChk( ConfigDAQmxCITask(dev, &errorInfo.errMsg) );
	
	// Configure CO DAQmx Task
	errChk( ConfigDAQmxCOTask(dev, &errorInfo.errMsg) );
	
Error:
	
RETURN_ERR	
}

static int ConfigDAQmxAITask (Dev_type* dev, char** errorMsg)
{
#define ConfigDAQmxAITask_Err_ChannelNotImplemented		-1

INIT_ERR
	
	ChanSet_type* 		chanSet			= NULL;
	
	if (!dev->AITaskSet) return 0; 		// do nothing
	
	// clear AI task if any
	if (dev->AITaskSet->taskHndl) {
		DAQmxErrChk( DAQmxClearTask(dev->AITaskSet->taskHndl) );
		dev->AITaskSet->taskHndl = NULL;
	}
	
	// clear and init readAIData used for processing incoming data
	discard_ReadAIData_type(&dev->AITaskSet->readAIData);
	nullChk( dev->AITaskSet->readAIData = init_ReadAIData_type(dev) );
	
	// check if there is at least one AI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nChans 			= ListNumItems(dev->AITaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->srcVChan)) {
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
	
	// create AI channels and adjust data type conversion
	nChans = ListNumItems(dev->AITaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		
		// adjust data type conversion
		if (chanSet->integrateFlag)
			AdjustAIDataTypeGainOffset(chanSet, dev->AITaskSet->timing->oversampling);
		else
			AdjustAIDataTypeGainOffset(chanSet, 1);
	
		// include in the task only channel for which HW-timing is required and which are open
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
		
		switch(chanSet->chanType) {
			
			case Chan_AI_Voltage:
				
				ChanSet_AI_Voltage_type*	AIVoltageChanSet	= (ChanSet_AI_Voltage_type*)chanSet;
				DAQmxErrChk ( DAQmxCreateAIVoltageChan(dev->AITaskSet->taskHndl, chanSet->name, "", AIVoltageChanSet->terminal, AIVoltageChanSet->VMin, AIVoltageChanSet->VMax, DAQmx_Val_Volts, NULL) ); 
				
				break;
				
			case Chan_AI_Current:
				
				ChanSet_AI_Current_type*	AICurrentChanSet	= (ChanSet_AI_Current_type*)chanSet;
				DAQmxErrChk ( DAQmxCreateAICurrentChan(dev->AITaskSet->taskHndl, chanSet->name, "", AICurrentChanSet->terminal, AICurrentChanSet->IMin, AICurrentChanSet->IMax, DAQmx_Val_Amps,
													  AICurrentChanSet->shuntResistorType, AICurrentChanSet->shuntResistorValue, NULL) );
							  
				break;
				
			// add here more channel type cases
			
			default:
				errorInfo.error 	= ConfigDAQmxAITask_Err_ChannelNotImplemented; 
				errorInfo.errMsg	= StrDup("Channel type not implemented.");
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
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_Rate, dev->AITaskSet->timing->sampleRate * dev->AITaskSet->timing->oversampling));
	
	// set operation mode: finite, continuous
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampQuant_SampMode, dev->AITaskSet->timing->measMode));
	
	// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
	// if a reference clock is given, use it to synchronize the internal clock
	if (dev->AITaskSet->timing->refClkSource && dev->AITaskSet->timing->refClkSource[0]) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_RefClk_Src, dev->AITaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_RefClk_Rate, dev->AITaskSet->timing->refClkFreq));
	}
	
	// adjust manually the number of samples to acquire and  input buffer size to be even multiple of blocksizes and make sure that the input buffer is
	// at least equal or larger than the total number of samples per channel to be acquired in case of finite acquisition and at least the number of
	// default samples for continuous acquisition (which is adjusted automatically depending on the sampling rate)
	
	switch (dev->AITaskSet->timing->measMode) {
		uInt32		quot;
		uInt32		defaultBuffSize; 
		
		case Operation_Finite:
			
			// set number of samples per channel
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->AITaskSet->timing->nSamples * dev->AITaskSet->timing->oversampling));
			quot = (dev->AITaskSet->timing->nSamples * dev->AITaskSet->timing->oversampling) / dev->AITaskSet->timing->blockSize; 
			if (quot % 2) quot++;
			if (!quot) quot = 1;
			DAQmxErrChk(DAQmxCfgInputBuffer (dev->AITaskSet->taskHndl, dev->AITaskSet->timing->blockSize * quot));
			break;
				
		case Operation_Continuous:
			
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
				if (dev->AITaskSet->startTrig->trigSource && dev->AITaskSet->startTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->slope, dev->AITaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AITaskSet->startTrig->trigSource && dev->AITaskSet->startTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->startTrig->trigSource, dev->AITaskSet->startTrig->wndTrigCond, 
							 dev->AITaskSet->startTrig->wndTop, dev->AITaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AITaskSet->startTrig->trigSource && dev->AITaskSet->startTrig->trigSource[0])
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
				if (dev->AITaskSet->referenceTrig->trigSource && dev->AITaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->slope, dev->AITaskSet->referenceTrig->level, dev->AITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AITaskSet->referenceTrig->trigSource && dev->AITaskSet->referenceTrig->trigSource[0])
				DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->AITaskSet->taskHndl, dev->AITaskSet->referenceTrig->trigSource, dev->AITaskSet->referenceTrig->wndTrigCond, 
							 dev->AITaskSet->referenceTrig->wndTop, dev->AITaskSet->referenceTrig->wndBttm, dev->AITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AITaskSet->referenceTrig->trigSource && dev->AITaskSet->referenceTrig->trigSource[0])
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
	// Signal routing
	//----------------------
	
	if (dev->AITaskSet->timing->startSignalRouting && dev->AITaskSet->timing->startSignalRouting[0])
		DAQmxErrChk( DAQmxSetExportedSignalAttribute(dev->AITaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm, dev->AITaskSet->timing->startSignalRouting) );
	else
		DAQmxErrChk( DAQmxResetExportedSignalAttribute(dev->AITaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm) );
	
	//----------------------  
	// Commit AI Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR
}

static int ConfigDAQmxAOTask (Dev_type* dev, char** errorMsg)
{
#define ConfigDAQmxAOTask_Err_OutOfMemory				-1
#define ConfigDAQmxAOTask_Err_ChannelNotImplemented		-2

INIT_ERR
	
	ChanSet_type* 		chanSet			= NULL;
	
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
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->sinkVChan)) {
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
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
	
		// include in the task only channels for which HW-timing is required
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->sinkVChan)) continue;
		
		// create channel
		switch(chanSet->chanType) {
			
			case Chan_AO_Voltage:
				
				ChanSet_AO_Voltage_type*	AOVoltageChanSet = (ChanSet_AO_Voltage_type*)chanSet;
				DAQmxErrChk (DAQmxCreateAOVoltageChan(dev->AOTaskSet->taskHndl, chanSet->name, "", AOVoltageChanSet->VMin, AOVoltageChanSet->VMax, DAQmx_Val_Volts, NULL)); 
				
				break;
				
			case Chan_AO_Current:
				
				ChanSet_AO_Current_type*	AOCurrentChanSet = (ChanSet_AO_Current_type*)chanSet;
				DAQmxErrChk (DAQmxCreateAOCurrentChan(dev->AOTaskSet->taskHndl, chanSet->name, "", AOCurrentChanSet->IMin, AOCurrentChanSet->IMax, DAQmx_Val_Amps, NULL));
							  
				break;
			/*	
			case Chan_AO_Function:
			
				DAQmxErrChk (DAQmxCreateAOFuncGenChan(dev->AOTaskSet->taskHndl, (*chanSetPtr)->name, "", ,
				
				break;
			*/
			
			default:
				
				errorInfo.error 	= ConfigDAQmxAOTask_Err_ChannelNotImplemented;
				nullChk( errorInfo.errMsg	= StrDup("Channel type not implemented") );
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
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampClk_Rate, dev->AOTaskSet->timing->sampleRate));
	
	// set operation mode: finite, continuous
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampQuant_SampMode, dev->AOTaskSet->timing->measMode));
	
	// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
	DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk)); 
	
	// set number of samples per channel for finite generation
	if (dev->AOTaskSet->timing->measMode == Operation_Finite)
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->AOTaskSet->timing->nSamples));
	
	// if a reference clock is given, use it to synchronize the internal clock
	if (dev->AOTaskSet->timing->refClkSource && dev->AOTaskSet->timing->refClkSource[0]) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_RefClk_Src, dev->AOTaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->AOTaskSet->taskHndl, DAQmx_RefClk_Rate, dev->AOTaskSet->timing->refClkFreq));
	}
	
	// disable AO regeneration
	DAQmxSetWriteAttribute (dev->AOTaskSet->taskHndl, DAQmx_Write_RegenMode, DAQmx_Val_DoNotAllowRegen);
	
	
	switch (dev->AOTaskSet->timing->measMode) {
		case Operation_Finite:
			// adjust output buffer size to match the number of samples to be generated   
		//	DAQmxErrChk ( DAQmxCfgOutputBuffer(dev->AOTaskSet->taskHndl, dev->AOTaskSet->timing->nSamples) );
		//	break;
			
		case Operation_Continuous:
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
				if (dev->AOTaskSet->startTrig->trigSource && dev->AOTaskSet->startTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->slope, dev->AOTaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AOTaskSet->startTrig->trigSource && dev->AOTaskSet->startTrig->trigSource[0]) 
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->startTrig->trigSource, dev->AOTaskSet->startTrig->wndTrigCond, 
							 dev->AOTaskSet->startTrig->wndTop, dev->AOTaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AOTaskSet->startTrig->trigSource && dev->AOTaskSet->startTrig->trigSource[0]) 
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
				if (dev->AOTaskSet->referenceTrig->trigSource && dev->AOTaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->slope, 
														 dev->AOTaskSet->referenceTrig->level, dev->AOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->AOTaskSet->referenceTrig->trigSource && dev->AOTaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->wndTrigCond, 
							 dev->AOTaskSet->referenceTrig->wndTop, dev->AOTaskSet->referenceTrig->wndBttm, dev->AOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->AOTaskSet->referenceTrig->trigSource && dev->AOTaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgDigEdgeRefTrig(dev->AOTaskSet->taskHndl, dev->AOTaskSet->referenceTrig->trigSource, dev->AOTaskSet->referenceTrig->slope, dev->AOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalPattern:
				break;
		}
	
	//----------------------
	// Add AO Task callbacks
	//----------------------
	// register AO data request callback if task is continuous
	//if (dev->AOTaskSet->timing->measMode == Operation_Continuous)
	DAQmxErrChk (DAQmxRegisterEveryNSamplesEvent(dev->AOTaskSet->taskHndl, DAQmx_Val_Transferred_From_Buffer, dev->AOTaskSet->timing->blockSize, 0, AODAQmxTaskDataRequest_CB, dev)); 
	// register AO task done event callback
	// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
	// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
	DAQmxErrChk (DAQmxRegisterDoneEvent(dev->AOTaskSet->taskHndl, 0, AODAQmxTaskDone_CB, dev));
	
	
	//----------------------
	// Signal routing
	//----------------------
	
	if (dev->AOTaskSet->timing->startSignalRouting && dev->AOTaskSet->timing->startSignalRouting[0])
		DAQmxErrChk( DAQmxSetExportedSignalAttribute(dev->AOTaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm, dev->AOTaskSet->timing->startSignalRouting) );
	else
		DAQmxErrChk( DAQmxResetExportedSignalAttribute(dev->AOTaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm) );
	
	//----------------------  
	// Commit AO Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR
}

static int ConfigDAQmxDITask (Dev_type* dev, char** errorMsg)
{
INIT_ERR

	ChanSet_type* 		chanSet		= NULL;
	
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
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DITaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->srcVChan)) {
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
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DITaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
		
		DAQmxErrChk (DAQmxCreateDIChan(dev->DITaskSet->taskHndl, chanSet->name, "", DAQmx_Val_ChanForAllLines)); 
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
	if (dev->DITaskSet->timing->refClkSource && dev->DITaskSet->timing->refClkSource[0]) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_RefClk_Src, dev->DITaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_RefClk_Rate, dev->DITaskSet->timing->refClkFreq));
	}
	
	// adjust manually number of samples and input buffer size to be even multiple of blocksizes and make sure that the input buffer is
	// at least equal or larger than the total number of samples per channel to be acquired in case of finite acquisition and at least the number of
	// default samples for continuous acquisition (which is adjusted automatically depending on the sampling rate)
	
	switch (dev->DITaskSet->timing->measMode) {
		int			quot;
		uInt32		defaultBuffSize; 
		
		case Operation_Finite:
			
			// set number of samples per channel read within one call of the read function, i.e. blocksize
			DAQmxErrChk (DAQmxSetTimingAttribute(dev->DITaskSet->taskHndl, DAQmx_SampQuant_SampPerChan, dev->DITaskSet->timing->nSamples));
			quot = dev->DITaskSet->timing->nSamples  / dev->DITaskSet->timing->blockSize; 
			if (quot % 2) quot++;
			DAQmxErrChk(DAQmxCfgInputBuffer (dev->DITaskSet->taskHndl, dev->DITaskSet->timing->blockSize * quot));
			break;
				
		case Operation_Continuous:
			
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
				if (dev->DITaskSet->startTrig->trigSource && dev->DITaskSet->startTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->slope, dev->DITaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DITaskSet->startTrig->trigSource && dev->DITaskSet->startTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->startTrig->trigSource, dev->DITaskSet->startTrig->wndTrigCond, 
						 		dev->DITaskSet->startTrig->wndTop, dev->DITaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DITaskSet->startTrig->trigSource && dev->DITaskSet->startTrig->trigSource[0])
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
				if (dev->DITaskSet->referenceTrig->trigSource && dev->DITaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->slope, 
														 dev->DITaskSet->referenceTrig->level, dev->DITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DITaskSet->referenceTrig->trigSource && dev->DITaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->DITaskSet->taskHndl, dev->DITaskSet->referenceTrig->trigSource, dev->DITaskSet->referenceTrig->wndTrigCond, 
								 dev->DITaskSet->referenceTrig->wndTop, dev->DITaskSet->referenceTrig->wndBttm, dev->DITaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DITaskSet->referenceTrig->trigSource && dev->DITaskSet->referenceTrig->trigSource[0])
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
	// Signal routing
	//----------------------
	
	if (dev->DITaskSet->timing->startSignalRouting && dev->DITaskSet->timing->startSignalRouting[0])
		DAQmxErrChk( DAQmxSetExportedSignalAttribute(dev->DITaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm, dev->DITaskSet->timing->startSignalRouting) );
	else
		DAQmxErrChk( DAQmxResetExportedSignalAttribute(dev->DITaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm) );
	
	
	//----------------------  
	// Commit DI Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->DITaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->DITaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->DITaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;

DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR
}

static int ConfigDAQmxDOTask (Dev_type* dev, char** errorMsg)
{
INIT_ERR	
	
	ChanSet_type* 		chanSet		= NULL;
	
	if (!dev->DOTaskSet) return 0; 		// do nothing
	
	// clear DO task if any
	if (dev->DOTaskSet->taskHndl) {
		DAQmxErrChk(DAQmxClearTask(dev->DOTaskSet->taskHndl));
		dev->DOTaskSet->taskHndl = NULL;
	}
	
	// clear and init writeDOData used for continuous streaming
	discard_WriteDOData_type(&dev->DOTaskSet->writeDOData);
	nullChk ( dev->DOTaskSet->writeDOData = init_WriteDOData_type(dev) );
	
	// check if there is at least one DO task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	size_t	nChans			= ListNumItems(dev->DOTaskSet->chanSet);
	for (size_t i = 1; i <= nChans; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->sinkVChan) ) {
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
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
	
		// include in the task only channel for which HW-timing is required
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->sinkVChan)) continue;
		
		DAQmxErrChk (DAQmxCreateDOChan(dev->DOTaskSet->taskHndl, chanSet->name, "", DAQmx_Val_ChanForAllLines)); 
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
	if (dev->DOTaskSet->timing->refClkSource && dev->DOTaskSet->timing->refClkSource[0]) {
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_RefClk_Src, dev->DOTaskSet->timing->refClkSource));
		DAQmxErrChk (DAQmxSetTimingAttribute(dev->DOTaskSet->taskHndl, DAQmx_RefClk_Rate, dev->DOTaskSet->timing->refClkFreq));
	}
	
	// set number of samples per channel for finite acquisition
	if (dev->DOTaskSet->timing->measMode == Operation_Finite)
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
				if (dev->DOTaskSet->startTrig->trigSource && dev->DOTaskSet->startTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->slope, dev->DOTaskSet->startTrig->level));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DOTaskSet->startTrig->trigSource && dev->DOTaskSet->startTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->startTrig->trigSource, dev->DOTaskSet->startTrig->wndTrigCond, 
						 		dev->DOTaskSet->startTrig->wndTop, dev->DOTaskSet->startTrig->wndBttm));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DOTaskSet->startTrig->trigSource && dev->DOTaskSet->startTrig->trigSource[0])
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
				if (dev->DOTaskSet->referenceTrig->trigSource && dev->DOTaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->slope, 
														 dev->DOTaskSet->referenceTrig->level, dev->DOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_AnalogWindow:
				if (dev->DOTaskSet->referenceTrig->trigSource && dev->DOTaskSet->referenceTrig->trigSource[0])
					DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(dev->DOTaskSet->taskHndl, dev->DOTaskSet->referenceTrig->trigSource, dev->DOTaskSet->referenceTrig->wndTrigCond, 
						 		dev->DOTaskSet->referenceTrig->wndTop, dev->DOTaskSet->referenceTrig->wndBttm, dev->DOTaskSet->referenceTrig->nPreTrigSamples));  
				break;
			
			case Trig_DigitalEdge:
				if (dev->DOTaskSet->referenceTrig->trigSource && dev->DOTaskSet->referenceTrig->trigSource[0])
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
	// Signal routing
	//----------------------
	
	if (dev->DOTaskSet->timing->startSignalRouting && dev->DOTaskSet->timing->startSignalRouting[0])
		DAQmxErrChk( DAQmxSetExportedSignalAttribute(dev->DOTaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm, dev->DOTaskSet->timing->startSignalRouting) );
	else
		DAQmxErrChk( DAQmxResetExportedSignalAttribute(dev->DOTaskSet->taskHndl, DAQmx_Exported_StartTrig_OutputTerm) );
	
	
	//----------------------  
	// Commit DO Task
	//----------------------  
	
	DAQmxErrChk( DAQmxTaskControl(dev->DOTaskSet->taskHndl, DAQmx_Val_Task_Verify) );
	DAQmxErrChk( DAQmxTaskControl(dev->DOTaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
	DAQmxErrChk( DAQmxTaskControl(dev->DOTaskSet->taskHndl, DAQmx_Val_Task_Commit) );
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR	
}

static int ConfigDAQmxCITask (Dev_type* dev, char** errorMsg)
{
#define ConfigDAQmxCITask_Err_ChanNotImplemented		-1 
	
INIT_ERR	
	
	ChanSet_type* 		chanSet			= NULL;
	size_t				nCIChan			= 0;
	char* 				taskName		= NULL;
	
	
	if (!dev->CITaskSet) return 0;	// do nothing
	
	// get number of CI channels
	nCIChan = ListNumItems(dev->CITaskSet->chanTaskSet); 
	
	// clear CI tasks
	errChk( ClearDAQmxCITasks(dev, &errorInfo.errMsg) );
	
	// check if there is at least one CI task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	for (size_t i = 1; i <= nCIChan; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		if (!chanSet->onDemand && IsVChanOpen((VChan_type*)chanSet->srcVChan)) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	
	// continue setting up the CI task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create CI channels
	for (size_t i = 1; i <= nCIChan; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
	
		// include in the task only channel for which HW-timing is required
		if (chanSet->onDemand && !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
		
		switch (chanSet->chanType) {
				
			case Chan_CI_Frequency:
				
				ChanSet_CI_Frequency_type*	chCIFreq = (ChanSet_CI_Frequency_type*)chanSet;
				
				// create DAQmx CI task for each counter
				taskName = GetTaskControlName(dev->taskController);
				AppendString(&taskName, " CI Task on ", -1);
				AppendString(&taskName, strstr(chanSet->name, "/")+1, -1);
				DAQmxErrChk(DAQmxCreateTask(taskName, &chCIFreq->baseClass.taskHndl) );
				OKfree(taskName);
				
				DAQmxErrChk ( DAQmxCreateCIFreqChan(chCIFreq->baseClass.taskHndl, chanSet->name, "", chCIFreq->freqMin, chCIFreq->freqMax, DAQmx_Val_Hz, 
												   chCIFreq->baseClass.activeEdge, chCIFreq->measMethod, chCIFreq->measTime, chCIFreq->divisor, NULL) );
				
				// use another terminal for the counter if other than default
				if (chCIFreq->freqInputTerminal)
				DAQmxErrChk ( DAQmxSetChanAttribute(chCIFreq->baseClass.taskHndl, chanSet->name, DAQmx_CI_Freq_Term, chCIFreq->freqInputTerminal) );
				
				// sample clock source
				// if no sample clock is given, then use OnboardClock by default
				if (chCIFreq->taskTiming.sampClkSource || chCIFreq->taskTiming.sampClkSource[0])
					DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_SampClk_Src, chCIFreq->taskTiming.sampClkSource) );
				else
					DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_SampClk_Src, "OnboardClock") );
					
				// sample clock edge
				DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_SampClk_ActiveEdge, chCIFreq->taskTiming.sampClkEdge) );
	
				// sampling rate
				DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_SampClk_Rate, chCIFreq->taskTiming.sampleRate) );
	
				// set operation mode: finite, continuous
				DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_SampQuant_SampMode, chCIFreq->taskTiming.measMode) );
	
				// set sample timing type: for now this is set to sample clock, i.e. samples are taken with respect to a clock signal
				DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_SampTimingType, DAQmx_Val_SampClk) );
				
				// set number of samples per channel for finite acquisition
				if (chCIFreq->taskTiming.measMode == Operation_Finite)
					DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_SampQuant_SampPerChan, chCIFreq->taskTiming.nSamples) );
	
				// if a reference clock is given, use it to synchronize the internal clock
				if (chCIFreq->taskTiming.refClkSource && chCIFreq->taskTiming.refClkSource[0]) {  
					DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_RefClk_Src, chCIFreq->taskTiming.refClkSource) );
					DAQmxErrChk ( DAQmxSetTimingAttribute(chCIFreq->baseClass.taskHndl, DAQmx_RefClk_Rate, chCIFreq->taskTiming.refClkFreq) );
				}
	
				//----------------------
				// Configure triggers
				//----------------------
				// configure start trigger
				if (chCIFreq->baseClass.startTrig)
					switch (chCIFreq->baseClass.startTrig->trigType) {
				
						case Trig_None:
							DAQmxErrChk (DAQmxDisableStartTrig(chCIFreq->baseClass.taskHndl));
							break;
			
						case Trig_AnalogEdge:
							if (chCIFreq->baseClass.startTrig->trigSource && chCIFreq->baseClass.startTrig->trigSource[0])
								DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(chCIFreq->baseClass.taskHndl, chCIFreq->baseClass.startTrig->trigSource, chCIFreq->baseClass.startTrig->slope, 
																	   chCIFreq->baseClass.startTrig->level));  
							break;
			
						case Trig_AnalogWindow:
							if (chCIFreq->baseClass.startTrig->trigSource && chCIFreq->baseClass.startTrig->trigSource[0])
								DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(chCIFreq->baseClass.taskHndl, chCIFreq->baseClass.startTrig->trigSource, chCIFreq->baseClass.startTrig->wndTrigCond, 
							 			     chCIFreq->baseClass.startTrig->wndTop, chCIFreq->baseClass.startTrig->wndBttm));  
							break;
			
						case Trig_DigitalEdge:
							if (chCIFreq->baseClass.startTrig->trigSource && chCIFreq->baseClass.startTrig->trigSource[0])
								DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(chCIFreq->baseClass.taskHndl, chCIFreq->baseClass.startTrig->trigSource, chCIFreq->baseClass.startTrig->slope));  
							break;
			
						case Trig_DigitalPattern:
							break;
					}
				
				// configure reference trigger
				if (chCIFreq->baseClass.referenceTrig)
					switch (chCIFreq->baseClass.referenceTrig->trigType) {
			
						case Trig_None:
							DAQmxErrChk (DAQmxDisableRefTrig(chCIFreq->baseClass.taskHndl));
							break;
			
						case Trig_AnalogEdge:
							if (chCIFreq->baseClass.referenceTrig->trigSource && chCIFreq->baseClass.referenceTrig->trigSource[0])
								DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(chCIFreq->baseClass.taskHndl, chCIFreq->baseClass.referenceTrig->trigSource, chCIFreq->baseClass.referenceTrig->slope, 
																	 chCIFreq->baseClass.referenceTrig->level, chCIFreq->baseClass.referenceTrig->nPreTrigSamples));  
							break;
			
						case Trig_AnalogWindow:
							if (chCIFreq->baseClass.referenceTrig->trigSource && chCIFreq->baseClass.referenceTrig->trigSource[0])
								DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(chCIFreq->baseClass.taskHndl,chCIFreq->baseClass.referenceTrig->trigSource, chCIFreq->baseClass.referenceTrig->wndTrigCond, 
							 				chCIFreq->baseClass.referenceTrig->wndTop, chCIFreq->baseClass.referenceTrig->wndBttm, chCIFreq->baseClass.referenceTrig->nPreTrigSamples));  
							break;
			
						case Trig_DigitalEdge:
							if (chCIFreq->baseClass.referenceTrig->trigSource && chCIFreq->baseClass.referenceTrig->trigSource[0])
							DAQmxErrChk (DAQmxCfgDigEdgeRefTrig(chCIFreq->baseClass.taskHndl, chCIFreq->baseClass.referenceTrig->trigSource, chCIFreq->baseClass.referenceTrig->slope, chCIFreq->baseClass.referenceTrig->nPreTrigSamples));  
							break;
			
						case Trig_DigitalPattern:
							break;
					}
	
				
				// commit DAQmxtask
				DAQmxErrChk( DAQmxTaskControl(chCIFreq->baseClass.taskHndl, DAQmx_Val_Task_Verify) );
				DAQmxErrChk( DAQmxTaskControl(chCIFreq->baseClass.taskHndl, DAQmx_Val_Task_Reserve) );
				DAQmxErrChk( DAQmxTaskControl(chCIFreq->baseClass.taskHndl, DAQmx_Val_Task_Commit) );
				break;
				
			default:
				
				errorInfo.error 	= ConfigDAQmxCITask_Err_ChanNotImplemented;
				errorInfo.errMsg	= StrDup("Channel type not implemented.");
				goto Error;
		}
		
	}
				 
	return 0;

DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR	
}


static int ConfigDAQmxCOTask (Dev_type* dev, char** errorMsg)
{
#define ConfigDAQmxCOTask_Err_ChanNotImplemented		-1 
	
INIT_ERR
	
	ChanSet_CO_type*	chanSet			= NULL;
	size_t				nCOChan			= 0;
	char*				taskName		= NULL;
	
	if (!dev->COTaskSet) return 0;	// do nothing
	
	// get number of CO channels
	nCOChan = ListNumItems(dev->COTaskSet->chanTaskSet); 
	
	// clear CO tasks
	for (size_t i = 1; i <= nCOChan; i++) {	
		chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		if (chanSet->taskHndl) {
			DAQmxErrChk( DAQmxClearTask(chanSet->taskHndl) );
			chanSet->taskHndl = NULL;
		}
	}
	
	// check if there is at least one CO task that requires HW-timing
	BOOL 	hwTimingFlag 	= FALSE;
	for (size_t i = 1; i <= nCOChan; i++) {	
		chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		if (!chanSet->baseClass.onDemand && IsVChanOpen((VChan_type*)chanSet->baseClass.sinkVChan)) {
			hwTimingFlag = TRUE;
			break;
		}
	}
	
	// continue setting up the CO task if any channels require HW-timing
	if (!hwTimingFlag) return 0;	// do nothing
	
	// create CO channels
	for (size_t i = 1; i <= nCOChan; i++) {	
		chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		// include in the task only channel for which HW-timing is required
		if (chanSet->baseClass.onDemand && !IsVChanOpen((VChan_type*)chanSet->baseClass.sinkVChan)) continue;
		
		// create DAQmx CO task for each counter 
		taskName = GetTaskControlName(dev->taskController);
		AppendString(&taskName, " CO Task on ", -1);
		AppendString(&taskName, strstr(chanSet->baseClass.name, "/")+1, -1);
		DAQmxErrChk (DAQmxCreateTask(taskName, &chanSet->taskHndl));
		OKfree(taskName);
		
		switch (chanSet->baseClass.chanType) {
				
			case Chan_CO_Pulse_Frequency:
				
				DAQmxErrChk ( DAQmxCreateCOPulseChanFreq(chanSet->taskHndl, chanSet->baseClass.name, "", DAQmx_Val_Hz, PulseTrainIdleStates_To_DAQmxVal(GetPulseTrainIdleState(chanSet->pulseTrain)), 
														 GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)chanSet->pulseTrain), GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)chanSet->pulseTrain), 
														 (GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)chanSet->pulseTrain)/100)) );
				break;
				
			case Chan_CO_Pulse_Time:
				     
				DAQmxErrChk ( DAQmxCreateCOPulseChanTime(chanSet->taskHndl, chanSet->baseClass.name, "", DAQmx_Val_Seconds, PulseTrainIdleStates_To_DAQmxVal(GetPulseTrainIdleState(chanSet->pulseTrain)), 
													GetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)chanSet->pulseTrain), GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)chanSet->pulseTrain), 
													GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)chanSet->pulseTrain)) );
				break;
				
			case Chan_CO_Pulse_Ticks:
				
				if (chanSet->clockSource && chanSet->clockSource[0])
					DAQmxErrChk ( DAQmxCreateCOPulseChanTicks(chanSet->taskHndl, chanSet->baseClass.name, "", chanSet->clockSource,
								 PulseTrainIdleStates_To_DAQmxVal(GetPulseTrainIdleState(chanSet->pulseTrain)), GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)chanSet->pulseTrain), GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)chanSet->pulseTrain),
								 GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)chanSet->pulseTrain)) );
				else
					DAQmxErrChk ( DAQmxCreateCOPulseChanTicks(chanSet->taskHndl, chanSet->baseClass.name, "", "OnboardClock",
							 PulseTrainIdleStates_To_DAQmxVal(GetPulseTrainIdleState(chanSet->pulseTrain)), GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)chanSet->pulseTrain), GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)chanSet->pulseTrain),
							 GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)chanSet->pulseTrain)) );
	
	
				break;
				
			default:
				
				errorInfo.error 	= ConfigDAQmxCOTask_Err_ChanNotImplemented;
				errorInfo.errMsg	= StrDup("Channel type not implemented");
				goto Error;
		}
		
		//----------------------
		// Configure CO triggers
		//----------------------
		// configure start trigger
		if (chanSet->startTrig)
			switch (chanSet->startTrig->trigType) {
				
				case Trig_None:
					DAQmxErrChk (DAQmxDisableStartTrig(chanSet->taskHndl));
					break;
			
				case Trig_AnalogEdge:
					if (chanSet->startTrig->trigSource && chanSet->startTrig->trigSource[0])
						DAQmxErrChk (DAQmxCfgAnlgEdgeStartTrig(chanSet->taskHndl, chanSet->startTrig->trigSource, chanSet->startTrig->slope, chanSet->startTrig->level));  
					break;
			
				case Trig_AnalogWindow:
					if (chanSet->startTrig->trigSource && chanSet->startTrig->trigSource[0])
						DAQmxErrChk (DAQmxCfgAnlgWindowStartTrig(chanSet->taskHndl, chanSet->startTrig->trigSource, chanSet->startTrig->wndTrigCond, 
									 chanSet->startTrig->wndTop, chanSet->startTrig->wndBttm));  
					break;
			
				case Trig_DigitalEdge:
					if (chanSet->startTrig->trigSource && chanSet->startTrig->trigSource[0])
						DAQmxErrChk (DAQmxCfgDigEdgeStartTrig(chanSet->taskHndl, chanSet->startTrig->trigSource, chanSet->startTrig->slope));  
					break;
			
				case Trig_DigitalPattern:
					break;
			}
	
		// configure reference trigger
		if (chanSet->referenceTrig)
			switch (chanSet->referenceTrig->trigType) {
			
				case Trig_None:
					DAQmxErrChk (DAQmxDisableRefTrig(chanSet->taskHndl));
					break;
			
				case Trig_AnalogEdge:
					if (chanSet->referenceTrig->trigSource && chanSet->referenceTrig->trigSource[0])
						DAQmxErrChk (DAQmxCfgAnlgEdgeRefTrig(chanSet->taskHndl, chanSet->referenceTrig->trigSource, chanSet->referenceTrig->slope, chanSet->referenceTrig->level, chanSet->referenceTrig->nPreTrigSamples));  
					break;
			
				case Trig_AnalogWindow:
					if (chanSet->referenceTrig->trigSource && chanSet->referenceTrig->trigSource[0])
						DAQmxErrChk (DAQmxCfgAnlgWindowRefTrig(chanSet->taskHndl, chanSet->referenceTrig->trigSource, chanSet->referenceTrig->wndTrigCond, 
							 		chanSet->referenceTrig->wndTop, chanSet->referenceTrig->wndBttm, chanSet->referenceTrig->nPreTrigSamples));  
					break;
			
				case Trig_DigitalEdge:
					if (chanSet->referenceTrig->trigSource && chanSet->referenceTrig->trigSource[0])
						DAQmxErrChk (DAQmxCfgDigEdgeRefTrig(chanSet->taskHndl, chanSet->referenceTrig->trigSource, chanSet->referenceTrig->slope, chanSet->referenceTrig->nPreTrigSamples));  
					break;
			
				case Trig_DigitalPattern:
					break;
			}
		
		
		DAQmxErrChk ( DAQmxCfgImplicitTiming(chanSet->taskHndl, PulseTrainModes_To_DAQmxVal(GetPulseTrainMode(chanSet->pulseTrain)), GetPulseTrainNPulses(chanSet->pulseTrain)) );
		DAQmxErrChk ( DAQmxSetTrigAttribute(chanSet->taskHndl, DAQmx_StartTrig_Retriggerable, TRUE) );    
		
		// register CO task done event callback
		// Registers a callback function to receive an event when a task stops due to an error or when a finite acquisition task or finite generation task completes execution.
		// A Done event does not occur when a task is stopped explicitly, such as by calling DAQmxStopTask.
		DAQmxErrChk( DAQmxRegisterDoneEvent(chanSet->taskHndl, 0, CODAQmxTaskDone_CB, dev) );
		
		//----------------------  
		// Commit CO Task
		//----------------------  
	
		DAQmxErrChk( DAQmxTaskControl(chanSet->taskHndl, DAQmx_Val_Task_Verify) );
		DAQmxErrChk( DAQmxTaskControl(chanSet->taskHndl, DAQmx_Val_Task_Reserve) );
		DAQmxErrChk( DAQmxTaskControl(chanSet->taskHndl, DAQmx_Val_Task_Commit) );
	}
	
	
	return 0;

DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR	
}

static int ClearDAQmxCITasks (Dev_type* dev, char** errorMsg) 
{
#define ClearDAQmxCITasks_Err_ChanNotImplemented		-1
	
INIT_ERR
	
	ChanSet_type* 	chanSet			= NULL;
	size_t			nCIChan			= 0;
	
	if (!dev->CITaskSet) return 0;	// do nothing
	
	// get number of CI channels
	nCIChan = ListNumItems(dev->CITaskSet->chanTaskSet); 
	
	// clear CI tasks
	for (size_t i = 1; i <= nCIChan; i++) {	
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		switch (chanSet->chanType) {
				
			case Chan_CI_Frequency:
				
				ChanSet_CI_Frequency_type* chanSet_CI_Frequency = (ChanSet_CI_Frequency_type*) chanSet;
				if (chanSet_CI_Frequency->baseClass.taskHndl) {
					DAQmxErrChk( DAQmxClearTask(chanSet_CI_Frequency->baseClass.taskHndl) );
					chanSet_CI_Frequency->baseClass.taskHndl = NULL;
				}
				break;
				
			case Chan_CI_EdgeCount:
				ChanSet_CI_EdgeCount_type* chanSet_CI_EdgeCount = (ChanSet_CI_EdgeCount_type*) chanSet;
				if (chanSet_CI_EdgeCount->baseClass.taskHndl) {
					DAQmxErrChk( DAQmxClearTask(chanSet_CI_EdgeCount->baseClass.taskHndl) );
					chanSet_CI_EdgeCount->baseClass.taskHndl = NULL;
				}
				break;
				
			default:
				
				errorInfo.error 	= ClearDAQmxCITasks_Err_ChanNotImplemented; 
				errorInfo.errMsg 	= StrDup("Channel not implemented");
				goto Error;
		}
		
		
	}
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
RETURN_ERR
}

static int ClearDAQmxTasks (Dev_type* dev, char** errorMsg)
{
INIT_ERR	
	
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
	
	errChk( ClearDAQmxCITasks(dev, NULL) );
	
	// CO task
	if (dev->COTaskSet) {
		ChanSet_CO_type* 	chanSet;
		size_t				nChans		= ListNumItems(dev->COTaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);	
			if (chanSet->taskHndl) {
				DAQmxClearTask(chanSet->taskHndl);
				chanSet->taskHndl = NULL;
			}
		}
	}
	
	// signal that device is not configured
	errChk( TaskControlEvent(dev->taskController, TC_Event_Unconfigure, NULL, NULL, &errorInfo.errMsg) );
	
Error:
	
RETURN_ERR
}

static int StopDAQmxTasks (Dev_type* dev, char** errorMsg)
{
#define StopDAQmxTasks_Err_StoppingTasks	-1
	
INIT_ERR	
	
	int*		nActiveTasksPtr		= NULL;
	bool32		taskDoneFlag		= FALSE;
	
	
	CmtErrChk( CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr) );
	// AI task
	if (dev->AITaskSet && dev->AITaskSet->taskHndl) {
		DAQmxErrChk( DAQmxIsTaskDone(dev->AITaskSet->taskHndl, &taskDoneFlag) );
		if (!taskDoneFlag ) {
			DAQmxErrChk( DAQmxStopTask(dev->AITaskSet->taskHndl) );
			(*nActiveTasksPtr)--;
			
			// send NULL data packets to AI channels used in the DAQmx task
			size_t				nChans		= ListNumItems(dev->AITaskSet->chanSet); 
			ChanSet_type*		chanSet		= NULL; 
			for (size_t i = 1; i <= nChans; i++) { 
				chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
				// include only channels for which HW-timing is required
				if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
				// send NULL packet to signal end of data transmission
				errChk( SendNullPacket(chanSet->srcVChan, &errorInfo.errMsg) );
			}
			
		}
	}
	
	// AO task (stop if finite; continuous task stopping happens on receiving a NULL packet and is handled by WriteAODAQmx)
	if (dev->AOTaskSet && dev->AOTaskSet->taskHndl && dev->AOTaskSet->timing->measMode == Operation_Finite) {
		DAQmxErrChk( DAQmxIsTaskDone(dev->AOTaskSet->taskHndl, &taskDoneFlag) ); 
		if (!taskDoneFlag ) {
			DAQmxErrChk( DAQmxStopTask(dev->AOTaskSet->taskHndl) );
			(*nActiveTasksPtr)--; 
		}
	}
	
	// DI task
	if (dev->DITaskSet && dev->DITaskSet->taskHndl) {
		DAQmxErrChk( DAQmxIsTaskDone(dev->DITaskSet->taskHndl, &taskDoneFlag) ); 
		if (!taskDoneFlag ) {
			DAQmxErrChk( DAQmxStopTask(dev->DITaskSet->taskHndl) );
			(*nActiveTasksPtr)--;
		}
	}
	
	// DO task
	if (dev->DOTaskSet && dev->DOTaskSet->taskHndl) {
		DAQmxErrChk( DAQmxIsTaskDone(dev->DOTaskSet->taskHndl, &taskDoneFlag) );
		if (!taskDoneFlag ) {
			DAQmxErrChk( DAQmxStopTask(dev->DOTaskSet->taskHndl) );
			(*nActiveTasksPtr)--;
		}
	}
	
	// CI task
	if (dev->CITaskSet) {
		ChanSet_CI_type* 	chanSet = NULL;
		size_t				nChans	= ListNumItems(dev->CITaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSet = *(ChanSet_CI_type**)ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if (chanSet->taskHndl) {
				DAQmxErrChk( DAQmxIsTaskDone(chanSet->taskHndl, &taskDoneFlag) ); 
				if (!taskDoneFlag ) {
					DAQmxErrChk( DAQmxStopTask(chanSet->taskHndl) );
					(*nActiveTasksPtr)--;
				}
			}
		}
	}
	
	// CO task
	if (dev->COTaskSet) {
		ChanSet_CO_type* 	chanSet	= NULL;
		size_t				nChans	= ListNumItems(dev->COTaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);	
			if (chanSet->taskHndl) {
				DAQmxErrChk( DAQmxIsTaskDone(chanSet->taskHndl, &taskDoneFlag) ); 
				if (!taskDoneFlag ) {
					DAQmxErrChk( DAQmxStopTask(chanSet->taskHndl) );
					(*nActiveTasksPtr)--;
				}
			}
		}
	}
	
	if (!*nActiveTasksPtr)
		errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
	CmtErrChk( CmtReleaseTSVPtr(dev->nActiveTasks) );
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

CmtError:
	
Cmt_ERR

Error:
	
RETURN_ERR	
}

int CVICALLBACK StartAIDAQmxTask_CB (void *functionData)
{
INIT_ERR	
	
	Dev_type*				dev					= functionData;
	DataPacket_type*		dataPacket			= NULL;
	void*					dataPacketData		= NULL;
	DLDataTypes				dataPacketType		= 0;
	uInt64*					nSamplesPtr			= NULL;
	double*					samplingRatePtr		= NULL;
	DSInfo_type*			dsInfo				= NULL;
	Iterator_type*			iterator			= GetTaskControlIterator(dev->taskController);
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	//--------------
	// Sampling rate (must be received before N samples!)
	//--------------
	if (IsVChanOpen((VChan_type*)dev->AITaskSet->timing->samplingRateSinkVChan)) { 
		errChk( GetDataPacket(dev->AITaskSet->timing->samplingRateSinkVChan, &dataPacket, &errorInfo.errMsg) );
		dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType);
		
		switch (dataPacketType) {
			case DL_Float:
				dev->AITaskSet->timing->sampleRate = (double)**(float**)dataPacketData;
				break;
				
			case DL_Double:
				dev->AITaskSet->timing->sampleRate = **(double**)dataPacketData;
				break;
		}
		
		// if oversampling must be adjusted automatically
		if (dev->AITaskSet->timing->oversamplingAuto) {
			dev->AITaskSet->timing->oversampling = (uInt32)(dev->AITaskSet->timing->targetSampleRate/dev->AITaskSet->timing->sampleRate);
			if (!dev->AITaskSet->timing->oversampling) dev->AITaskSet->timing->oversampling = 1;
			SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_Oversampling, dev->AITaskSet->timing->oversampling);
				
			// update also read AI data structures
			discard_ReadAIData_type(&dev->AITaskSet->readAIData);
			dev->AITaskSet->readAIData = init_ReadAIData_type(dev->AITaskSet->dev);
		}
		
		// update sampling rate in dev structure
		DAQmxErrChk(DAQmxSetTimingAttribute(dev->AITaskSet->taskHndl, DAQmx_SampClk_Rate, dev->AITaskSet->timing->sampleRate * dev->AITaskSet->timing->oversampling));
		// update sampling rate in UI
		SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_SamplingRate, dev->AITaskSet->timing->sampleRate/1000);	// display in [kHz]
		// update duration in UI
		SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_Duration, dev->AITaskSet->timing->nSamples / dev->AITaskSet->timing->sampleRate);
		// given oversampling, adjust also the actual DAQ sampling rate display
		SetCtrlVal(dev->AITaskSet->timing->settingsPanHndl, Set_ActualSamplingRate, dev->AITaskSet->timing->sampleRate * dev->AITaskSet->timing->oversampling * 1e-3);	// display in [kHz]
		// cleanup
		ReleaseDataPacket(&dataPacket);
	}
	
	//----------
	// N samples
	//----------
	if (dev->AITaskSet->timing->measMode == Operation_Finite) {
		if (IsVChanOpen((VChan_type*)dev->AITaskSet->timing->nSamplesSinkVChan)) {
			errChk( GetDataPacket(dev->AITaskSet->timing->nSamplesSinkVChan, &dataPacket, &errorInfo.errMsg) );
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
				case DL_UInt64:
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
	
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Send task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	//----------
	// N samples
	//----------
	nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
	*nSamplesPtr = dev->AITaskSet->timing->nSamples;
	nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
	dataPacket = init_DataPacket_type(DL_UInt64, (void**) &nSamplesPtr, &dsInfo, NULL);
	errChk(SendDataPacket(dev->AITaskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg));
	
	//--------------
	// Sampling rate
	//--------------
	nullChk( samplingRatePtr = malloc(sizeof(double)) );
	*samplingRatePtr = dev->AITaskSet->timing->sampleRate;
	nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
	dataPacket = init_DataPacket_type(DL_Double, (void**) &samplingRatePtr, &dsInfo, NULL);
	errChk(SendDataPacket(dev->AITaskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg));
	
	
	// reset AI data processing structure
	discard_ReadAIData_type(&dev->AITaskSet->readAIData);
	nullChk( dev->AITaskSet->readAIData = init_ReadAIData_type(dev) );
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Start task as a function of HW trigger dependencies
	//-------------------------------------------------------------------------------------------------------------------------------
	
	errChk(WaitForHWTrigArmedSlaves(dev->AITaskSet->HWTrigMaster, &errorInfo.errMsg));
	
	DAQmxErrChk(DAQmxTaskControl(dev->AITaskSet->taskHndl, DAQmx_Val_Task_Start));
	
	errChk(SetHWTrigSlaveArmedStatus(dev->AITaskSet->HWTrigSlave, &errorInfo.errMsg));
	
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	discard_DSInfo_type(&dsInfo);
	
	StopDAQmxTasks(dev, NULL);

PRINT_ERR
	return 0;
}

int CVICALLBACK StartAODAQmxTask_CB (void *functionData)
{
#define StartAODAQmxTask_CB_Err_NumberOfReceivedSamplesNotTheSameAsTask		-1

INIT_ERR
	
	Dev_type*			dev						= functionData;
	DataPacket_type*	dataPacket				= NULL;
	void*				dataPacketData			= NULL;
	DLDataTypes			dataPacketType			= 0;
	uInt64*				nSamplesPtr				= NULL;
	double*				samplingRatePtr			= NULL;
	Waveform_type*		AOWaveform				= NULL;
	float64*			AOData 					= NULL;
	Iterator_type*		iterator				= GetTaskControlIterator(dev->taskController);
	DSInfo_type*		dsInfo					= NULL;
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive task settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	
	//----------
	// N samples
	//----------
	if (dev->AOTaskSet->timing->measMode == Operation_Finite) {
		if (IsVChanOpen((VChan_type*)dev->AOTaskSet->timing->nSamplesSinkVChan)) {
			errChk( GetDataPacket(dev->AOTaskSet->timing->nSamplesSinkVChan, &dataPacket, &errorInfo.errMsg) );
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
				case DL_UInt64:
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
	if (IsVChanOpen((VChan_type*)dev->AOTaskSet->timing->samplingRateSinkVChan)) { 
		errChk( GetDataPacket(dev->AOTaskSet->timing->samplingRateSinkVChan, &dataPacket, &errorInfo.errMsg) );
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
		SetCtrlVal(dev->AOTaskSet->timing->settingsPanHndl, Set_SamplingRate, dev->AOTaskSet->timing->sampleRate/1000);	// display in [kHz]
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
	nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
	dataPacket = init_DataPacket_type(DL_UInt64, (void**) &nSamplesPtr, &dsInfo, NULL);
	errChk(SendDataPacket(dev->AOTaskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg));
	
	//--------------
	// Sampling rate
	//--------------
	nullChk( samplingRatePtr = malloc(sizeof(double)) );
	*samplingRatePtr = dev->AOTaskSet->timing->sampleRate;
	nullChk( dsInfo = GetIteratorDSData(iterator, WAVERANK) );
	dataPacket = init_DataPacket_type(DL_Double, (void**) &samplingRatePtr, &dsInfo, NULL);
	errChk(SendDataPacket(dev->AOTaskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg));
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Fill output buffer
	//-------------------------------------------------------------------------------------------------------------------------------
	
	// clear streaming data structure
	discard_WriteAOData_type(&dev->AOTaskSet->writeAOData);
	nullChk( dev->AOTaskSet->writeAOData = init_WriteAOData_type(dev) );
	// output buffer is twice the writeblock, thus write two writeblocks
	errChk( WriteAODAQmx(dev, &errorInfo.errMsg) );
	errChk( WriteAODAQmx(dev, &errorInfo.errMsg) );
			
	//-------------------------------------------------------------------------------------------------------------------------------
	// Start task as a function of HW trigger dependencies
	//-------------------------------------------------------------------------------------------------------------------------------
	
	// wait for other HW triggered slaved to be armed
	errChk(WaitForHWTrigArmedSlaves(dev->AOTaskSet->HWTrigMaster, &errorInfo.errMsg));
	
	// launch task (and arm if this task is a slave triggered task)
	DAQmxErrChk(DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Start));
	
	// if this task has a master HW-trigger, then signal it that task is armed
	errChk(SetHWTrigSlaveArmedStatus(dev->AOTaskSet->HWTrigSlave, &errorInfo.errMsg));
	
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	OKfree(AOData);
	discard_Waveform_type(&AOWaveform);
	
	StopDAQmxTasks(dev, NULL);
	
PRINT_ERR

	return 0;
}

int CVICALLBACK StartDIDAQmxTask_CB (void *functionData)
{
	return 0;	
}

int CVICALLBACK StartDODAQmxTask_CB (void *functionData)
{
	return 0;	
}

int CVICALLBACK StartCIDAQmxTasks_CB (void *functionData)
{
	return 0;	
}

int CVICALLBACK StartCODAQmxTasks_CB (void *functionData)
{
INIT_ERR
	
	ChanSet_CO_type*	chanSetCO			= functionData;
	Dev_type*			dev 				= chanSetCO->baseClass.device;
	DataPacket_type*	dataPacket			= NULL;
	void*				dataPacketData		= NULL;
	DLDataTypes			dataPacketDataType	= 0;
	PulseTrain_type*	pulseTrain			= NULL;
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive task settings data and update UI settings
	//-------------------------------------------------------------------------------------------------------------------------------
	if (IsVChanOpen((VChan_type*)chanSetCO->baseClass.sinkVChan)) {
		errChk( GetDataPacket(chanSetCO->baseClass.sinkVChan, &dataPacket, &errorInfo.errMsg) );
		dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketDataType);
		discard_PulseTrain_type(&chanSetCO->pulseTrain);
		nullChk( chanSetCO->pulseTrain = CopyPulseTrain(*(PulseTrain_type**)dataPacketData) ); 
	
		// get channel panel handle to adjust UI settings
		int						timingPanHndl	= 0;
		
		GetPanelHandleFromTabPage(chanSetCO->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl); 
		
		double 					initialDelay		= 0;
		double					frequency			= 0;
		double					dutyCycle			= 0;
		double					lowTime				= 0;
		double					highTime			= 0;
		uInt32					initialDelayTicks	= 0;
		uInt32					lowTicks			= 0;
		uInt32					highTicks			= 0;
		PulseTrainIdleStates	idleState			= 0;
		PulseTrainModes			pulseMode			= 0;
		uInt64					nPulses				= 0;
		int						ctrlIdx				= 0;
		
		// set idle state
		idleState = GetPulseTrainIdleState(chanSetCO->pulseTrain);
		DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_IdleState, PulseTrainIdleStates_To_DAQmxVal(idleState)) );
		// generation mode
		pulseMode = GetPulseTrainMode(chanSetCO->pulseTrain);
		// timing
		nPulses = GetPulseTrainNPulses(chanSetCO->pulseTrain);
		DAQmxErrChk ( DAQmxCfgImplicitTiming(chanSetCO->taskHndl, PulseTrainModes_To_DAQmxVal(pulseMode), nPulses) );
		
		switch (dataPacketDataType) {
			case DL_PulseTrain_Freq:
				// set initial delay
				initialDelay = GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq_InitialDelay, initialDelay) );
				SetCtrlVal(timingPanHndl, COFreqPan_InitialDelay, initialDelay); 
				// frequency
				frequency = GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq, frequency) );
				SetCtrlVal(timingPanHndl, COFreqPan_Frequency, frequency); 
				// duty cycle
				dutyCycle = GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_DutyCyc, dutyCycle/100) ); // normalize from [%] to 0-1 range
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
				GetIndexFromValue(timingPanHndl, COFreqPan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(pulseMode)); 
				SetCtrlIndex(timingPanHndl, COFreqPan_Mode, ctrlIdx);
				break;
				
			case DL_PulseTrain_Time:
				// set initial delay
				initialDelay = GetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Time_InitialDelay, initialDelay) );
				SetCtrlVal(timingPanHndl, COTimePan_InitialDelay, initialDelay); 
				// low time
				lowTime = GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTime, lowTime) );
				SetCtrlVal(timingPanHndl, COTimePan_LowTime, lowTime); 
				// high time
				highTime = GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTime, highTime) );
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
				GetIndexFromValue(timingPanHndl, COTimePan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(pulseMode)); 
				SetCtrlIndex(timingPanHndl, COTimePan_Mode, ctrlIdx);
				break;
			
			case DL_PulseTrain_Ticks:
				// tick delay
				initialDelayTicks = GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Ticks_InitialDelay, initialDelayTicks) );
				SetCtrlVal(timingPanHndl, COTicksPan_InitialDelay, initialDelayTicks); 
				// low ticks
				lowTicks = GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTicks, lowTicks) );
				SetCtrlVal(timingPanHndl, COTicksPan_LowTicks, lowTicks); 
				// high ticks
				highTicks = GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
				DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTicks, highTicks) );
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
				GetIndexFromValue(timingPanHndl, COTicksPan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(pulseMode)); 
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
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) );  
			break;
			
		case PulseTrain_Time:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) ); 
			break;
			
		case PulseTrain_Ticks:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) ); 
			break;
		
	}
	
	errChk( SendDataPacket(chanSetCO->baseClass.srcVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Start task as a function of HW trigger dependencies
	//-------------------------------------------------------------------------------------------------------------------------------
	
	errChk(WaitForHWTrigArmedSlaves(chanSetCO->HWTrigMaster, &errorInfo.errMsg));
	
	DAQmxErrChk(DAQmxTaskControl(chanSetCO->taskHndl, DAQmx_Val_Task_Start));
	
	errChk(SetHWTrigSlaveArmedStatus(chanSetCO->HWTrigSlave, &errorInfo.errMsg));
	
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	discard_PulseTrain_type(&pulseTrain);
	
	StopDAQmxTasks(dev, NULL);
	
PRINT_ERR

	return 0;
}

//---------------------------------------------------------------------------------------------------------------------
// DAQmx task callbacks
//---------------------------------------------------------------------------------------------------------------------

static int SendAIBufferData (Dev_type* dev, ChanSet_type* AIChSet, size_t chIdx, int nRead, float64* AIReadBuffer, char** errorMsg) 
{
#define SendAIBufferData_Err_NotImplemented		-1
	
INIT_ERR

	double*				waveformData_double		= NULL;
	float*				waveformData_float		= NULL;
	uInt8*				waveformData_uInt8		= NULL;
	uInt16*				waveformData_uInt16		= NULL;
	uInt32*				waveformData_uInt32		= NULL;
	Waveform_type*		waveform				= NULL; 
	DataPacket_type*	dataPacket				= NULL;
	float64*			integrationBuffer		= dev->AITaskSet->readAIData->intBuffers[chIdx];
	uInt32				integration				= dev->AITaskSet->timing->oversampling;
	uInt32				nItegratedSamples		= (nRead + dev->AITaskSet->readAIData->nIntBuffElem[chIdx]) / integration;  // number of samples integrated per channel
	uInt32				nRemainingSamples		= (nRead + dev->AITaskSet->readAIData->nIntBuffElem[chIdx]) % integration;	// number of samples remaining to be integrated per channel
	uInt32				nBufferSamples			= 0;	
	uInt32				i						= 0;
	uInt32 				j						= 0;
	uInt32				k						= 0;
	Iterator_type* 		currentiter				= GetTaskControlIterator(dev->taskController);
	DSInfo_type*		dsInfo					= NULL;		// indexing info
	
	
	
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
				waveformData_double[j] += integrationBuffer[k];
				k += integration;
			}							   
		}
	 else 
		// jump over oversampled samples
		for (i = 0; i < nItegratedSamples; i++)
			waveformData_double[i] = integrationBuffer[i*integration];
			
			
	// shift unprocessed samples from the end of the buffer to its beginning
	if (nRemainingSamples)
		memmove(integrationBuffer, integrationBuffer + (nBufferSamples - nRemainingSamples), nRemainingSamples * sizeof(float64));
			
	dev->AITaskSet->readAIData->nIntBuffElem[chIdx] = nRemainingSamples;
	
	//------------------------------------- 
	// transform data and send data packet
	//-------------------------------------
	
	switch(GetSourceVChanDataType(AIChSet->srcVChan)) {
				
		case DL_Waveform_Double:
			
			for (i = 0; i < nItegratedSamples; i++)
				waveformData_double[i] = waveformData_double[i] * AIChSet->dataTypeConversion.gain + AIChSet->dataTypeConversion.offset;
			
			// prepare data packet
			nullChk( waveform 	= init_Waveform_type(Waveform_Double, dev->AITaskSet->timing->sampleRate, nItegratedSamples, (void**)&waveformData_double) );
			nullChk( dsInfo = GetIteratorDSData(currentiter, WAVERANK) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_Double, (void**) &waveform, &dsInfo,(DiscardFptr_type) discard_Waveform_type) ); 
			break;
				
		case DL_Waveform_Float:
			
			nullChk( waveformData_float = malloc(nItegratedSamples * sizeof(float)) );
			for (i = 0; i < nItegratedSamples; i++)
				waveformData_float[i] = (float) (waveformData_double[i] * AIChSet->dataTypeConversion.gain + AIChSet->dataTypeConversion.offset);
				
			// prepare data packet
			nullChk( waveform = init_Waveform_type(Waveform_Float, dev->AITaskSet->timing->sampleRate, nItegratedSamples, (void**)&waveformData_float) );
			nullChk( dsInfo = GetIteratorDSData(currentiter, WAVERANK) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_Float, (void**) &waveform, &dsInfo, (DiscardFptr_type) discard_Waveform_type) );
			break;
		
			
		case DL_Waveform_UInt:
			
			nullChk( waveformData_uInt32 = malloc(nItegratedSamples * sizeof(uInt32)) );
			for (i = 0; i < nItegratedSamples; i++)
				waveformData_uInt32[i] = (uInt32) (waveformData_double[i] * AIChSet->dataTypeConversion.gain + AIChSet->dataTypeConversion.offset);
			
			// prepare data packet
			nullChk( waveform = init_Waveform_type(Waveform_UInt, dev->AITaskSet->timing->sampleRate, nItegratedSamples, (void**)&waveformData_uInt32) );
			nullChk( dsInfo = GetIteratorDSData(currentiter, WAVERANK) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UInt, (void**) &waveform, &dsInfo, (DiscardFptr_type) discard_Waveform_type) );
			break;
				
		case DL_Waveform_UShort:
			
			nullChk( waveformData_uInt16 = malloc(nItegratedSamples * sizeof(uInt16)) );
			for (i = 0; i < nItegratedSamples; i++)
				waveformData_uInt16[i] = (uInt16) (waveformData_double[i] * AIChSet->dataTypeConversion.gain + AIChSet->dataTypeConversion.offset);
			
			// prepare data packet
			nullChk( waveform = init_Waveform_type(Waveform_UShort, dev->AITaskSet->timing->sampleRate, nItegratedSamples, (void**)&waveformData_uInt16) );
			nullChk( dsInfo = GetIteratorDSData(currentiter, WAVERANK) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UShort, (void**) &waveform, &dsInfo, (DiscardFptr_type) discard_Waveform_type) );
				 
			break;
				
		case DL_Waveform_UChar:
				
			nullChk( waveformData_uInt8 = malloc(nItegratedSamples * sizeof(uInt8)) );
			for (i = 0; i < nItegratedSamples; i++)
				waveformData_uInt8[i] = (uInt8) (waveformData_double[i] * AIChSet->dataTypeConversion.gain + AIChSet->dataTypeConversion.offset);	
			
			// prepare data packet
			nullChk( waveform = init_Waveform_type(Waveform_UChar, dev->AITaskSet->timing->sampleRate, nItegratedSamples, (void**)&waveformData_uInt8) );
			nullChk( dsInfo = GetIteratorDSData(currentiter, WAVERANK) );
			nullChk( dataPacket = init_DataPacket_type(DL_Waveform_UChar, (void**) &waveform,  &dsInfo, (DiscardFptr_type)discard_Waveform_type) );
			break;
		
		
		default:
			
			errorInfo.error		= SendAIBufferData_Err_NotImplemented;
			errorInfo.errMsg 	= StrDup("Not implemented"); 
			goto Error;
					
	}

	// cleanup
	OKfree(waveformData_double);
	
	//-------------------------------
	// send data packet with waveform
	//-------------------------------
	
	errChk( SendDataPacket(AIChSet->srcVChan, &dataPacket, 0, &errorInfo.errMsg) );
	
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
	discard_DSInfo_type(&dsInfo);
	
RETURN_ERR	
}

int32 CVICALLBACK AIDAQmxTaskDataAvailable_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
INIT_ERR
	
	Dev_type*			dev 					= callbackData;
	float64*    		readBuffer				= NULL;				// temporary buffer to place data into
	uInt32				nAI						= 0;
	int					nRead					= 0;
	ChanSet_type*		chanSet					= NULL;
	size_t				nChans					= ListNumItems(dev->AITaskSet->chanSet);
	size_t				chIdx					= 0;
	
	// allocate memory to read samples
	DAQmxErrChk( DAQmxGetTaskAttribute(taskHandle, DAQmx_Task_NumChans, &nAI) );
	nullChk( readBuffer = malloc(nSamples * nAI * sizeof(float64)) );
	
	// read samples from the AI buffer
	DAQmxErrChk( DAQmxReadAnalogF64(taskHandle, nSamples, dev->AITaskSet->timeout, DAQmx_Val_GroupByChannel, readBuffer, nSamples * nAI, &nRead, NULL) );
	
	for (size_t i = 1; i <= nChans; i++) {
		chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		// include only channels for which HW-timing is required
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
		// forward data from the AI buffer to the VChan
		errChk( SendAIBufferData(dev, chanSet, chIdx, nRead, readBuffer, &errorInfo.errMsg) ); 
		// next AI channel
		chIdx++;
	}
	
	// cleanup
	OKfree(readBuffer);
	
	// stop AI task if TC iteration was aborted
	if (GetTaskControlAbortFlag(dev->taskController)) {
		int*				nActiveTasksPtr		= NULL;
		
		DAQmxErrChk( DAQmxStopTask(dev->AITaskSet->taskHndl) );
		
		CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
		
			(*nActiveTasksPtr)--;
		
			if (!*nActiveTasksPtr)
				errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
		CmtReleaseTSVPtr(dev->nActiveTasks);
			
		// send NULL data packets to AI channels used in the DAQmx task
		for (size_t i = 1; i <= nChans; i++) { 
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			// include only channels for which HW-timing is required
			if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
			// send NULL packet to signal end of data transmission
			errChk( SendNullPacket(chanSet->srcVChan, &errorInfo.errMsg) );
		}
			
	}
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:

// cleanup
	OKfree(readBuffer);  
	
	StopDAQmxTasks(dev, NULL); 
	
PRINT_ERR
	
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition or generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK AIDAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
INIT_ERR

	Dev_type*			dev 					= callbackData;
	uInt32				nSamples				= 0;						// number of samples per channel in the AI buffer
	float64*    		readBuffer				= NULL;						// temporary buffer to place data into       
	uInt32				nAI						= 0;
	int					nRead					= 0;
	ChanSet_type*		chanSet					= NULL;
	size_t				nChans					= ListNumItems(dev->AITaskSet->chanSet);
	int*				nActiveTasksPtr			= NULL;
	size_t				chIdx					= 0;
	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	if (status < 0) goto DAQmxError;
	
	// get all read samples from the input buffer 
	DAQmxErrChk( DAQmxGetReadAttribute(taskHandle, DAQmx_Read_AvailSampPerChan, &nSamples) ); 
	
	// if there are no samples left in the buffer, send NULL data packet and stop here, otherwise read them out
	if (!nSamples) {
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			// include only channels for which HW-timing is required
			if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
			
			// send NULL packet to signal end of data transmission
			errChk( SendNullPacket(chanSet->srcVChan, &errorInfo.errMsg) ); 
		}
		// stop the Task
		DAQmxErrChk( DAQmxTaskControl(taskHandle, DAQmx_Val_Task_Stop) );
		
		// Task Controller iteration is complete if all DAQmx Tasks are complete
		CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
		
			(*nActiveTasksPtr)--;
		
			if (!*nActiveTasksPtr)
				errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
		CmtReleaseTSVPtr(dev->nActiveTasks);
	
		return 0;
	}
	
	// allocate memory for samples
	DAQmxErrChk( DAQmxGetTaskAttribute(taskHandle, DAQmx_Task_NumChans, &nAI) );
	nullChk( readBuffer = malloc(nSamples * nAI * sizeof(float64)) );
	
	// read remaining samples from the AI buffer
	DAQmxErrChk( DAQmxReadAnalogF64(taskHandle, -1, dev->AITaskSet->timeout, DAQmx_Val_GroupByChannel , readBuffer, nSamples * nAI, &nRead, NULL) );
	
	for (size_t i = 1; i <= nChans; i++) {
		chanSet = *(ChanSet_type**) ListGetPtrToItem(dev->AITaskSet->chanSet, i);
		// include only channels for which HW-timing is required
		if (chanSet->onDemand || !IsVChanOpen((VChan_type*)chanSet->srcVChan)) continue;
		// forward data from AI buffer to the VChan 
		errChk( SendAIBufferData(dev, chanSet, chIdx, nRead, readBuffer, &errorInfo.errMsg) ); 
		// send NULL packet to signal end of data transmission
		errChk( SendNullPacket(chanSet->srcVChan, &errorInfo.errMsg) );
		// next AI channel
		chIdx++;
	}
	
	// stop the Task
	DAQmxErrChk( DAQmxTaskControl(taskHandle, DAQmx_Val_Task_Stop) );
		
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
	
		(*nActiveTasksPtr)--;
		
		if (!*nActiveTasksPtr)
			errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
	CmtReleaseTSVPtr(dev->nActiveTasks);
	
	OKfree(readBuffer);
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	OKfree(readBuffer);
	
	StopDAQmxTasks(dev, NULL); 
	
PRINT_ERR
	
	return 0;
}

int32 CVICALLBACK AODAQmxTaskDataRequest_CB (TaskHandle taskHandle, int32 everyNsamplesEventType, uInt32 nSamples, void *callbackData)
{
INIT_ERR

	Dev_type*			dev 					= callbackData;
	

	errChk( WriteAODAQmx(dev, &errorInfo.errMsg) );
		
	return 0;
	
Error:
	
	// stop all tasks
	StopDAQmxTasks(dev, NULL);
	
	OKfree(errorInfo.errMsg);
	return 0;
}

// Called only if a running task encounters an error or stops by itself in case of a finite acquisition of generation task. 
// It is not called if task stops after calling DAQmxClearTask or DAQmxStopTask.
int32 CVICALLBACK AODAQmxTaskDone_CB (TaskHandle taskHandle, int32 status, void *callbackData)
{
INIT_ERR

	Dev_type*	dev 			= callbackData;
	int			nActiveTasks    = 0;
	int*		nActiveTasksPtr = &nActiveTasks;
	
	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	DAQmxErrChk(status);
	
	// stop task explicitly
	DAQmxErrChk(DAQmxStopTask(taskHandle));
	
	// clear and init writeAOData used for continuous streaming
	discard_WriteAOData_type(&dev->AOTaskSet->writeAOData);
	nullChk( dev->AOTaskSet->writeAOData = init_WriteAOData_type(dev) );
	
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
	(*nActiveTasksPtr)--;
		
	if (!*nActiveTasksPtr)
		errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
	CmtReleaseTSVPtr(dev->nActiveTasks);
		
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	StopDAQmxTasks(dev, NULL);
	
PRINT_ERR

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
	
	return 0;
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
INIT_ERR

	Dev_type*			dev 			= callbackData;
	int*				nActiveTasksPtr	= NULL;
	
	// in case of error abort all tasks and finish Task Controller iteration with an error
	if (status < 0) goto DAQmxError;
	
	// stop the Task
	DAQmxErrChk( DAQmxTaskControl(taskHandle, DAQmx_Val_Task_Stop) );
		
	// Task Controller iteration is complete if all DAQmx Tasks are complete
	CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
	(*nActiveTasksPtr)--;
		
	if (!*nActiveTasksPtr)
		errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
	CmtReleaseTSVPtr(dev->nActiveTasks);
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	StopDAQmxTasks(dev, NULL);
	
PRINT_ERR

	return 0;
}

//--------------------------------------------------------------------------------------------
// DAQmx module and VChan data exchange
//--------------------------------------------------------------------------------------------

/// HIFN Writes writeblock number of samples to the AO task.
static int WriteAODAQmx (Dev_type* dev, char** errorMsg) 
{
#define	WriteAODAQmx_Err_WaitingForDataTimeout		-1
#define WriteAODAQmx_Err_DataTypeNotSupported		-2
#define WriteAODAQmx_Err_NULLReceivedBeforeData		-3
	
INIT_ERR

	DataPacket_type* 		dataPacket									= NULL;
	DLDataTypes				dataPacketType								= 0;
	void*					dataPacketData								= NULL;
	double*					waveformData								= NULL;
	float*					floatWaveformData							= NULL;
	WriteAOData_type*    	data            							= dev->AOTaskSet->writeAOData;
	size_t          		queue_items									= 0;
	size_t          		ncopies										= 0;	// number of datapacket copies to fill at least a writeblock size
	double					nRepeats									= 1;
	float64*        		tmpbuff										= NULL;
	CmtTSQHandle			tsqID										= 0;
	char					CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE]	= "";
	int 					buffsize									= 0;
	int						itemsRead									= 0;
	int						nSamplesWritten								= 0;
	BOOL					stopAOTaskFlag								= TRUE;
	int*					nActiveTasksPtr 							= NULL; 
	BOOL					writeLastBlock								= FALSE;
	int 					LastBlockSize								= data->writeblock;  
	div_t					numblocks									;

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
						//int*		nActiveTasksPtr	= NULL;
					
						DAQmxErrChk( DAQmxStopTask(dev->AOTaskSet->taskHndl) );
						// Task Controller iteration is complete if all DAQmx Tasks are complete
						CmtErrChk( CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr) );
						(*nActiveTasksPtr)--;
		
						if (!*nActiveTasksPtr)
							errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
						CmtErrChk( CmtReleaseTSVPtr(dev->nActiveTasks) );
						
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
				if (!itemsRead)
					SET_ERR(WriteAODAQmx_Err_WaitingForDataTimeout, "Waiting for AO data timed out.");
				
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
						
						SET_ERR(WriteAODAQmx_Err_DataTypeNotSupported, "Data type not supported.");
						
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
	
	// if in continouous mode and all AO channels received a NULL packet, stop AO task
	if (dev->AOTaskSet->timing->measMode==Operation_Continuous){
		for (int j = 0; j < data->numchan; j++) {
			if (!data->nullPacketReceived[j]) {
				stopAOTaskFlag = FALSE;
				break;
			}
		}
	}
	else {
		//in finite mode; stopping on correct number of samples written
		stopAOTaskFlag = FALSE; 
	}

	if (stopAOTaskFlag){
		//determine num blocks to write
		for (int j = 0; j < data->numchan; j++) {  
			numblocks=div(data->databuff_size[j],data->writeblock);
			if (numblocks.quot==0) {
				writeLastBlock=TRUE;
				if (numblocks.rem<LastBlockSize) LastBlockSize=numblocks.rem;   //size of block is smallest data size of a channel
			}
			else {
				 //nothing; this channel has enough data for a next writeblock
			}
		}
	}
	
	if ((stopAOTaskFlag && writeLastBlock) || GetTaskControlAbortFlag(dev->taskController)) {
		int*	nActiveTasksPtr = NULL;
		
		// write last block with adjusted size
		DAQmxErrChk(DAQmxWriteAnalogF64(dev->AOTaskSet->taskHndl, LastBlockSize, 0, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, data->dataout, &nSamplesWritten, NULL)); 
		
		DAQmxErrChk( DAQmxStopTask(dev->AOTaskSet->taskHndl) );
		// Task Controller iteration is complete if all DAQmx Tasks are complete
		CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr);
		
			(*nActiveTasksPtr)--;
		
			if (!*nActiveTasksPtr)
				errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
		CmtReleaseTSVPtr(dev->nActiveTasks);
		
	} 
	else
		DAQmxErrChk(DAQmxWriteAnalogF64(dev->AOTaskSet->taskHndl, data->writeblock, 0, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, data->dataout, &nSamplesWritten, NULL));
				 
	
	return 0; // no error
	
DAQmxError:
	
DAQmx_ERROR_INFO

CmtError:
	
Cmt_ERR

Error:
	
	// cleanup
	ReleaseDataPacket(&dataPacket);
	
RETURN_ERR
}

/*
/// HIFN Writes writeblock number of samples to the DO task.
static int WriteDODAQmx(Dev_type* dev, char** errorMsg) 
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
	
DAQmx_ERROR_INFO

CmtError:
	
Cmt_ERR

Error:

RETURN_ERR
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

//--------------------------------------------
// DAQmx conversion functions
//--------------------------------------------

static int32 PulseTrainIdleStates_To_DAQmxVal (PulseTrainIdleStates idleVal)
{
	switch (idleVal) {
			
		case PulseTrainIdle_Low:
			return DAQmx_Val_Low;
			
		case PulseTrainIdle_High:
			return DAQmx_Val_High;
	}
}

static int32 PulseTrainModes_To_DAQmxVal (PulseTrainModes pulseMode)
{
	switch (pulseMode) {
			
		case PulseTrain_Finite:
			return DAQmx_Val_FiniteSamps;
			
		case PulseTrain_Continuous:
			return DAQmx_Val_ContSamps;
	}
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
INIT_ERR

	NIDAQmxManager_type* 	nidaq 		= (NIDAQmxManager_type*) callbackData;
	
	// clear table
	DeleteTableRows(nidaq->devListPanHndl, DevListPan_DAQTable, 1, -1);
	// display device properties table
	DisplayPanel(nidaq->devListPanHndl);
	
	// find devices
	DiscardDeviceList(&allInstalledDevices);
	SetCtrlAttribute (nidaq->devListPanHndl, DevListPan_DAQTable, ATTR_LABEL_TEXT, "Searching for devices...");
	ProcessDrawEvents();
	nullChk( allInstalledDevices = ListAllDAQmxDevices() );
	if (ListNumItems(allInstalledDevices)) 
		SetCtrlAttribute(nidaq->devListPanHndl, DevListPan_AddBTTN, ATTR_DIMMED, 0); 
	
	UpdateDeviceList(allInstalledDevices, nidaq->devListPanHndl, DevListPan_DAQTable);
	
Error:
	
PRINT_ERR

	return;
}
 
static int AddDAQmxDeviceToManager (Dev_type* dev) 
{
	int						error 				= 0;
	int						tabPageIdx			= 0;
	char*					tabName				= NULL;
	char*					tcName				= GetTaskControlName(dev->taskController);
	void*					panelCallbackData	= NULL;
	int						panHndl				= 0;
	int						ioVal				= 0;
	int						ioMode				= 0;
	
	// register device task controller with the framework
	DLAddTaskController((DAQLabModule_type*)dev->niDAQModule, dev->taskController);
	
	// copy DAQmx Task settings panel to module tab and get handle to the panel inserted in the tab
	tabPageIdx = InsertPanelAsTabPage(dev->niDAQModule->mainPanHndl, NIDAQmxPan_Devices, -1, dev->niDAQModule->taskSetPanHndl);
	GetPanelHandleFromTabPage(dev->niDAQModule->mainPanHndl, NIDAQmxPan_Devices, tabPageIdx, &dev->devPanHndl);
	// change tab title to new Task Controller name
	tabName = StrDup(dev->attr->name);
	AppendString(&tabName, ": ", -1);
	AppendString(&tabName, tcName, -1);
	SetTabPageAttribute(dev->niDAQModule->mainPanHndl, NIDAQmxPan_Devices, tabPageIdx, ATTR_LABEL_TEXT, tabName);
	OKfree(tabName);
	// remove "None" labelled Tab (always first tab) if its panel doesn't have callback data attached to it  
	GetPanelHandleFromTabPage(dev->niDAQModule->mainPanHndl, NIDAQmxPan_Devices, 0, &panHndl);
	GetPanelAttribute(panHndl, ATTR_CALLBACK_DATA, &panelCallbackData); 
	if (!panelCallbackData) DeleteTabPage(dev->niDAQModule->mainPanHndl, NIDAQmxPan_Devices, 0, 1);
	// connect DAQmx data and user interface callbackFn to all direct controls in the new panel 
	SetCtrlsInPanCBInfo(dev, DAQmxDevTaskSet_CB, dev->devPanHndl);
	// connect DAQmx data to the panel as well
	SetPanelAttribute(dev->devPanHndl, ATTR_CALLBACK_DATA, dev);
	// cleanup
	OKfree(tcName);
	
	//------------------------------------------------------------------------------------------------
	// populate controls based on the device capabilities
	//------------------------------------------------------------------------------------------------
					
	// acquire
	if (ListNumItems(dev->attr->AIchan) || ListNumItems(dev->attr->DIlines) || 
		ListNumItems(dev->attr->DIports) || ListNumItems(dev->attr->CIchan)) 
		InsertListItem(dev->devPanHndl, TaskSetPan_IO, -1, "Acquire", DAQmxAcquire);  
					
	// generate
	if (ListNumItems(dev->attr->AOchan) || ListNumItems(dev->attr->DOlines) || 
		ListNumItems(dev->attr->DOports) || ListNumItems(dev->attr->COchan)) 
		InsertListItem(dev->devPanHndl, TaskSetPan_IO, -1, "Generate", DAQmxGenerate);  
				
	GetCtrlVal(dev->devPanHndl, TaskSetPan_IO, &ioVal);
	PopulateIOMode(dev, dev->devPanHndl, TaskSetPan_IOMode, ioVal);
	GetCtrlVal(dev->devPanHndl, TaskSetPan_IOMode, &ioMode);
	PopulateIOType(dev, dev->devPanHndl, TaskSetPan_IOType, ioVal, ioMode);
	
	//------------------------------------------------------------------------------------------------
	// add channels
	//------------------------------------------------------------------------------------------------
	PopulateChannels(dev);
	
	//------------------------------------------------------------------------------------------------
	// initialize Task Controller UI controls
	//------------------------------------------------------------------------------------------------
	// set repeats and change data type to size_t
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DATA_TYPE, VAL_SIZE_T);
	SetCtrlVal(dev->devPanHndl, TaskSetPan_Repeat, GetTaskControlIterations(dev->taskController)); 
	// chage data type of iterations to size_t
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_TotalIterations, ATTR_DATA_TYPE, VAL_SIZE_T);
	// set wait
	SetCtrlVal(dev->devPanHndl, TaskSetPan_Wait, GetTaskControlIterationsWait(dev->taskController));
	// set mode
	SetCtrlVal(dev->devPanHndl, TaskSetPan_Mode, !GetTaskControlMode(dev->taskController));
	// dim repeat if TC mode is continuous, otherwise undim
	if (GetTaskControlMode(dev->taskController) == TASK_FINITE) 
		// finite
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, FALSE);
	else
		// continuous
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, TRUE);
	
	
	return 0;
	
Error:
	
	return error;
	
}

int CVICALLBACK ManageDevices_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case DevListPan_AddBTTN:
					
					NIDAQmxManager_type* 	nidaq				 	= (NIDAQmxManager_type*) callbackData;
					char*					newTCName				= NULL;
					Dev_type*				newDAQmxDev				= NULL;
					DevAttr_type*			newDAQmxDevAttrPtr		= NULL;
					
					// create new Task Controller for the DAQmx device
					newTCName = DLGetUINameInput ("New DAQmx Task Controller", DAQLAB_MAX_TASKCONTROLLER_NAME_NCHARS, ValidTaskControllerName, NULL);
					if (!newTCName) return 0; // operation cancelled, do nothing	
					
					// copy device attributes
					newDAQmxDevAttrPtr = copy_DevAttr_type(*(DevAttr_type**)ListGetPtrToItem(allInstalledDevices, currDev));
					// add new DAQmx device to module list and framework
					if (!(newDAQmxDev = init_Dev_type(nidaq, &newDAQmxDevAttrPtr, newTCName))) {
						discard_DevAttr_type(&newDAQmxDevAttrPtr);
						OKfree(newTCName);
						DLMsg(OutOfMemoryMsg, 1);
						return 0;
					}
					
					ListInsertItem(nidaq->DAQmxDevices, &newDAQmxDev, END_OF_LIST);
					
					AddDAQmxDeviceToManager(newDAQmxDev);
					
					// configure Task Controller
					errChk( TaskControlEvent(newDAQmxDev->taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
					
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
	
Error:
	
PRINT_ERR

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
		ChanSet_type*		chanSet		= NULL;
		size_t				nChans 		= ListNumItems(dev->AITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) chanSet->srcVChan);
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
		ChanSet_type*		chanSet		= NULL;
		size_t				nChans 		= ListNumItems(dev->AOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule , (VChan_type*) chanSet->sinkVChan);
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
		ChanSet_type*	chanSet		= NULL;
		size_t 			nChans 		= ListNumItems(dev->DITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DITaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) chanSet->srcVChan);
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
		ChanSet_type*	chanSet		= NULL;
		size_t			nChans 		= ListNumItems(dev->DOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
			DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) chanSet->sinkVChan);
		}
	}
	
	//----------- 
	// CI Task
	//-----------
	
	if (dev->CITaskSet) {
		// CI channels
		ChanSet_CI_type*	chanSet		= NULL;
		size_t				nChans 		= ListNumItems(dev->CITaskSet->chanTaskSet); 
		for (size_t i = 1; i <= nChans; i++) {  
			chanSet = *(ChanSet_CI_type**)ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
		
			if (chanSet->baseClass.srcVChan)
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) chanSet->baseClass.srcVChan);   
		
			if (chanSet->baseClass.sinkVChan) 
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) chanSet->baseClass.sinkVChan);   
		}
	}
	
	//----------- 
	// CO Task
	//-----------
	
	if (dev->COTaskSet) {
		// CO channels
		ChanSet_CO_type*	chanSet		= NULL;
		size_t				nChans 		= ListNumItems(dev->COTaskSet->chanTaskSet); 
		for (size_t i = 1; i <= nChans; i++) {  
			chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
		
			// Sink & Source VChans used to exchange pulse train info
			if (chanSet->baseClass.srcVChan)
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) chanSet->baseClass.srcVChan);   
		
			if (chanSet->baseClass.sinkVChan) 
				DLUnregisterVChan((DAQLabModule_type*)dev->niDAQModule, (VChan_type*) chanSet->baseClass.sinkVChan);
			
			// HW triggers
			if (chanSet->HWTrigMaster)
				DLUnregisterHWTrigMaster(chanSet->HWTrigMaster);
			
			if (chanSet->HWTrigSlave)
				DLUnregisterHWTrigSlave(chanSet->HWTrigSlave);
			
			
		}
	}
	
}

//---------------------------------------------------------------------------------------------------------------------------
// Trigger settings callbacks
//---------------------------------------------------------------------------------------------------------------------------

static int CVICALLBACK TaskStartTrigType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR	
	
	TaskTrig_type* 		taskTrig	 	= callbackData;
	int 				trigType;
	
	if (event != EVENT_COMMIT) return 0; // stop here if there are other events
	
	// get trigger type
	GetCtrlVal(panel, control,  &trigType);
	taskTrig->trigType = (Trig_type)trigType;
	
	AddStartTriggerToUI(taskTrig);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );             
	
Error:
	
PRINT_ERR

	return 0;
}

static void AddStartTriggerToUI (TaskTrig_type* taskTrig)
{
	int		ctrlIdx = 0;
	
	// clear all possible controls except the select trigger type control
	if (taskTrig->levelCtrlID) 					{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->levelCtrlID); taskTrig->levelCtrlID = 0;} 
	if (taskTrig->trigSlopeCtrlID)				{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID); taskTrig->trigSlopeCtrlID = 0;} 
	if (taskTrig->trigSourceCtrlID)				{NIDAQmx_DiscardIOCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID); taskTrig->trigSourceCtrlID = 0;}
	if (taskTrig->windowTrigCondCtrlID)			{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID); taskTrig->windowTrigCondCtrlID = 0;} 
	if (taskTrig->windowBottomCtrlID)			{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID); taskTrig->windowBottomCtrlID = 0;} 
	if (taskTrig->windowTopCtrlID)				{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID); taskTrig->windowTopCtrlID = 0;}
	
	// load resources
	int DigEdgeTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig1);
	int DigPatternTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig2);
	int AnEdgeTrig_PanHndl	 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig3);
	int AnWindowTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, StartTrig4);
		
	// add controls based on the selected trigger type
	switch (taskTrig->trigType) {
					
		case Trig_None:
			
			break;
					
		case Trig_DigitalEdge:
			
			// trigger slope
			taskTrig->trigSlopeCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, StartTrig1_Slope, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
			GetIndexFromValue(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, &ctrlIdx, taskTrig->slope);
			SetCtrlIndex(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ctrlIdx);
			// trigger source
			taskTrig->trigSourceCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, StartTrig1_Source, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
			// turn string control into terminal control to select the trigger source
			NIDAQmx_NewTerminalCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, 0); // single terminal selection 
			// adjust trigger terminal control properties
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
			if (taskTrig->trigSource)
				SetCtrlVal(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, taskTrig->trigSource);
			
			break;
					
		case Trig_DigitalPattern:
					
			// not yet implemented, but similarly, here controls should be copied to the new taskTrig->trigPanHndl
			break;
					
		case Trig_AnalogEdge:
					
			// trigger slope
			taskTrig->trigSlopeCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, StartTrig3_Slope, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
			GetIndexFromValue(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, &ctrlIdx, taskTrig->slope);
			SetCtrlIndex(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ctrlIdx);
			// level
			taskTrig->levelCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, StartTrig3_Level, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->levelCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->levelCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerLevel_CB );
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->levelCtrlID, taskTrig->level);
			// trigger source
			taskTrig->trigSourceCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, StartTrig3_Source, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
			// turn string control into terminal control to select the trigger source
			NIDAQmx_NewTerminalCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, 0); // single terminal selection 
			// adjust trigger terminal control properties
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
			if (taskTrig->trigSource)
				SetCtrlVal(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, taskTrig->trigSource);
			
			break;
					
		case Trig_AnalogWindow:
				
			// window type
			taskTrig->windowTrigCondCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_WndType, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowType_CB);
			InsertListItem(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, 0, "Entering", TrigWnd_Entering); 
			InsertListItem(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, 1, "Leaving", TrigWnd_Leaving);
			GetIndexFromValue(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, &ctrlIdx, taskTrig->wndTrigCond);
			SetCtrlIndex(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, ctrlIdx);
			// window top
			taskTrig->windowTopCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_TrigWndTop, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowTop_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID, taskTrig->wndTop);
			// window bottom
			taskTrig->windowBottomCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_TrigWndBttm, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowBttm_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID, taskTrig->wndBttm);
			// trigger source
			taskTrig->trigSourceCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, StartTrig4_Source, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
			// turn string control into terminal control to select the trigger source
			NIDAQmx_NewTerminalCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, 0); // single terminal selection 
			// adjust trigger terminal control properties
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
			if (taskTrig->trigSource)
				SetCtrlVal(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, taskTrig->trigSource);
			
			break;
	}
			
	// discard resources
	DiscardPanel(DigEdgeTrig_PanHndl);
	DiscardPanel(DigPatternTrig_PanHndl);
	DiscardPanel(AnEdgeTrig_PanHndl);
	DiscardPanel(AnWindowTrig_PanHndl);
	
}

static int CVICALLBACK TaskReferenceTrigType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskTrig_type* 		taskTrig	= callbackData;
	int					trigType	= 0;

	if (event != EVENT_COMMIT) return 0; // stop here if event is not commit			
		 				
			
	// get trigger type
			
	GetCtrlVal(panel, control, &trigType);
	taskTrig->trigType = trigType;
			
	AddReferenceTriggerToUI(taskTrig);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );            
	
Error:
	
PRINT_ERR

	return 0;
}

static void AddReferenceTriggerToUI (TaskTrig_type* taskTrig)
{
	int ctrlIdx = 0;
	
	// clear all possible controls except the select trigger type control
	if (taskTrig->levelCtrlID) 				{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->levelCtrlID); taskTrig->levelCtrlID = 0;} 
	if (taskTrig->preTrigDurationCtrlID) 	{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID); taskTrig->preTrigDurationCtrlID = 0;} 
	if (taskTrig->preTrigNSamplesCtrlID) 	{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID); taskTrig->preTrigNSamplesCtrlID = 0;} 
	if (taskTrig->trigSlopeCtrlID)			{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID); taskTrig->trigSlopeCtrlID = 0;} 
	if (taskTrig->trigSourceCtrlID)			{NIDAQmx_DiscardIOCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID); taskTrig->trigSourceCtrlID = 0;} 
	if (taskTrig->windowTrigCondCtrlID)		{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID); taskTrig->windowTrigCondCtrlID = 0;} 
	if (taskTrig->windowBottomCtrlID)		{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID); taskTrig->windowBottomCtrlID = 0;} 
	if (taskTrig->windowTopCtrlID)			{DiscardCtrl(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID); taskTrig->windowTopCtrlID = 0;} 
			
	// load resources
	int DigEdgeTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig1);
	int DigPatternTrig_PanHndl	 	= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig2);
	int AnEdgeTrig_PanHndl	 		= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig3);
	int AnWindowTrig_PanHndl 		= LoadPanel(0, MOD_NIDAQmxManager_UI, RefTrig4);
			
	// add controls based on the selected trigger type
	switch (taskTrig->trigType) {
				
		case Trig_None:
			break;
					
		case Trig_DigitalEdge:
					
			// trigger slope
			taskTrig->trigSlopeCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_Slope, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
			GetIndexFromValue(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, &ctrlIdx, taskTrig->slope);
			SetCtrlIndex(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ctrlIdx);
			// pre-trigger samples
			taskTrig->preTrigNSamplesCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_NSamples, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_UNSIGNED_INTEGER);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigSamples_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, taskTrig->nPreTrigSamples);
			// pre-trigger duration
			taskTrig->preTrigDurationCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_Duration, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigDuration_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, taskTrig->nPreTrigSamples/(*taskTrig->samplingRate));  // display in [s]
			// trigger source
			taskTrig->trigSourceCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig1_Source, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
			// turn string control into terminal control to select the trigger source
			NIDAQmx_NewTerminalCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, 0); // single terminal selection 
			// adjust trigger terminal control properties
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
			if (taskTrig->trigSource)
				SetCtrlVal(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, taskTrig->trigSource); 
			break;
					
		case Trig_DigitalPattern:
					
			// not yet implemented, but similarly, here controls should be copied to the new taskTrig->trigPanHndl
			break;
			
		case Trig_AnalogEdge:
					
			// trigger slope
			taskTrig->trigSlopeCtrlID = DuplicateCtrl(DigEdgeTrig_PanHndl, RefTrig3_Slope, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSlope_CB);
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 0, "Rising", TrigSlope_Rising); 
			InsertListItem(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, 1, "Falling", TrigSlope_Falling);
			GetIndexFromValue(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, &ctrlIdx, taskTrig->slope);
			SetCtrlIndex(taskTrig->trigPanHndl, taskTrig->trigSlopeCtrlID, ctrlIdx);
			// level
			taskTrig->levelCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_Level, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->levelCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->levelCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerLevel_CB );
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->levelCtrlID, taskTrig->level);
			// pre-trigger samples
			taskTrig->preTrigNSamplesCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_NSamples, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_UNSIGNED_INTEGER);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigSamples_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, taskTrig->nPreTrigSamples);
			// pre-trigger duration
			taskTrig->preTrigDurationCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_Duration, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigDuration_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, taskTrig->nPreTrigSamples/(*taskTrig->samplingRate));  // display in [s]
			// trigger source
			taskTrig->trigSourceCtrlID = DuplicateCtrl(AnEdgeTrig_PanHndl, RefTrig3_Source, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
			// turn string control into terminal control to select the trigger source
			NIDAQmx_NewTerminalCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, 0); // single terminal selection 
			// adjust trigger terminal control properties
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
			if (taskTrig->trigSource)
				SetCtrlVal(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, taskTrig->trigSource);
			break;
					
		case Trig_AnalogWindow:
					
			// window type
			taskTrig->windowTrigCondCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_WndType, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowType_CB);
			InsertListItem(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, 0, "Entering", TrigWnd_Entering); 
			InsertListItem(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, 1, "Leaving", TrigWnd_Leaving);
			GetIndexFromValue(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, &ctrlIdx, taskTrig->wndTrigCond);
			SetCtrlIndex(taskTrig->trigPanHndl, taskTrig->windowTrigCondCtrlID, ctrlIdx);
			// window top
			taskTrig->windowTopCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_TrigWndTop, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowTop_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->windowTopCtrlID, taskTrig->wndTop);
			// window bottom
			taskTrig->windowBottomCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_TrigWndBttm, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION);  
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerWindowBttm_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->windowBottomCtrlID, taskTrig->wndBttm);
			// pre-trigger samples
			taskTrig->preTrigNSamplesCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_NSamples, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_DATA_TYPE, VAL_UNSIGNED_INTEGER);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigSamples_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->preTrigNSamplesCtrlID, taskTrig->nPreTrigSamples);
			// pre-trigger duration
			taskTrig->preTrigDurationCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_Duration, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerPreTrigDuration_CB);
			SetCtrlVal(taskTrig->trigPanHndl, taskTrig->preTrigDurationCtrlID, taskTrig->nPreTrigSamples/(*taskTrig->samplingRate));  // display in [s]
			// trigger source
			taskTrig->trigSourceCtrlID = DuplicateCtrl(AnWindowTrig_PanHndl, RefTrig4_Source, taskTrig->trigPanHndl, 0, VAL_KEEP_SAME_POSITION, VAL_KEEP_SAME_POSITION); 
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_DATA, taskTrig);
			SetCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, ATTR_CALLBACK_FUNCTION_POINTER, TriggerSource_CB);					
			// turn string control into terminal control to select the trigger source
			NIDAQmx_NewTerminalCtrl(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, 0); // single terminal selection 
			// adjust trigger terminal control properties
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_Limit_To_Device, 0);
			NIDAQmx_SetTerminalCtrlAttribute(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, NIDAQmx_IOCtrl_TerminalAdvanced, 1);
			if (taskTrig->trigSource)
				SetCtrlVal(taskTrig->trigPanHndl, taskTrig->trigSourceCtrlID, taskTrig->trigSource); 
			break;
	}
			
	// discard resources
	DiscardPanel(DigEdgeTrig_PanHndl);
	DiscardPanel(DigPatternTrig_PanHndl);
	DiscardPanel(AnEdgeTrig_PanHndl);
	DiscardPanel(AnWindowTrig_PanHndl);
}

static int CVICALLBACK TriggerSlope_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskTrig_type* 		taskTrig 		= callbackData;    
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events
		
	GetCtrlVal(panel, control, &taskTrig->slope);

	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK TriggerLevel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskTrig_type* 		taskTrig 		= callbackData;    
	
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events
	
	GetCtrlVal(panel, control, &taskTrig->level);
	
	// configure device
	errChk( ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );
	
Error:

PRINT_ERR

	return 0;
}

static int CVICALLBACK TriggerSource_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR	
	
	TaskTrig_type* 		taskTrig 		= callbackData;    
	int			 		buffsize		= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events 
	
	GetCtrlAttribute(panel, control, ATTR_STRING_TEXT_LENGTH, &buffsize);
	OKfree(taskTrig->trigSource);
	nullChk( taskTrig->trigSource = malloc((buffsize + 1) * sizeof(char)) ); // including ASCII null  
	GetCtrlVal(panel, control, taskTrig->trigSource);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK TriggerWindowType_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR	
	
	TaskTrig_type* 		taskTrig 	= callbackData;    
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events    
  
	GetCtrlVal(panel, control, &taskTrig->wndTrigCond);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK TriggerWindowBttm_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskTrig_type* 		taskTrig 	= callbackData;    
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events  
	
	GetCtrlVal(panel, control, &taskTrig->wndBttm);
	
	// configure device
	errChk( ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );

Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK TriggerWindowTop_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{ 
INIT_ERR

	TaskTrig_type* 		taskTrig 	= callbackData;    
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events 

	GetCtrlVal(panel, control, &taskTrig->wndTop);
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK TriggerPreTrigDuration_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskTrig_type* 		taskTrig 	= callbackData;    
	double				duration	= 0;
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events  
	
	GetCtrlVal(panel, control, &duration);
	// calculate number of samples given sampling rate
	taskTrig->nPreTrigSamples = (uInt32) (*taskTrig->samplingRate * duration);
	SetCtrlVal(panel, taskTrig->preTrigNSamplesCtrlID, taskTrig->nPreTrigSamples);
	// recalculate duration to match the number of samples and sampling rate
	SetCtrlVal(panel, control, taskTrig->nPreTrigSamples/(*taskTrig->samplingRate)); 
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR
	
	return 0;
}

static int CVICALLBACK TriggerPreTrigSamples_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	TaskTrig_type* 		taskTrig 	= callbackData;    
	
	if (event != EVENT_COMMIT) return 0;	 // do nothing for other events    
	
	GetCtrlVal(panel, control, &taskTrig->nPreTrigSamples);
	// recalculate duration
	SetCtrlVal(panel, taskTrig->preTrigDurationCtrlID, taskTrig->nPreTrigSamples/(*taskTrig->samplingRate)); 
	
	// configure device
	errChk(ConfigDAQmxDevice(taskTrig->device, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return 0;
}

//-----------------------------------------------------------------------------------------------------
// VChan Callbacks
//-----------------------------------------------------------------------------------------------------

static void AIDataVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR

	ChanSet_type* 	chanSet = VChanOwner;
	Dev_type*		dev		= chanSet->device;
	
	switch (state) {
			
		case VChan_Open:
			
			dev->AITaskSet->nOpenChannels++;
			break;
			
		case VChan_Closed:
			
			dev->AITaskSet->nOpenChannels--;
			break;
	}
	
	// Activate/Inactivate HW-triggered slave mode if task will be launched, i.e. if it has open data VChans
	if (dev->AITaskSet->nOpenChannels)
		SetHWTrigSlaveActive(dev->AITaskSet->HWTrigSlave, TRUE);
	else
		SetHWTrigSlaveActive(dev->AITaskSet->HWTrigSlave, FALSE);
	
	// update device settings
	errChk( ConfigDAQmxAITask(dev, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return;
}

static void AODataVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR
	
	ChanSet_type* 	chanSet = VChanOwner;
	Dev_type*		dev		= chanSet->device;
	
	switch (state) {
			
		case VChan_Open:
			
			dev->AOTaskSet->nOpenChannels++;
			break;
			
		case VChan_Closed:
			
			dev->AOTaskSet->nOpenChannels--;
			break;
	}
	
	// Activate/Inactivate HW-triggered slave mode if task will be launched, i.e. if it has open data VChans
	if (dev->AOTaskSet->nOpenChannels)
		SetHWTrigSlaveActive(dev->AOTaskSet->HWTrigSlave, TRUE);
	else
		SetHWTrigSlaveActive(dev->AOTaskSet->HWTrigSlave, FALSE);
	
	// update device settings
	errChk( ConfigDAQmxAOTask(dev, &errorInfo.errMsg) );
	
Error:
	
PRINT_ERR

	return;
}
	
static void ADNSamplesSinkVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
	ADTaskSet_type* 	tskSet	= VChanOwner;
	
	switch (state) {
			
		case VChan_Open:
			
			// dim number of samples controls and duration
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DIMMED, 1);
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Duration, ATTR_DIMMED, 1);
			break;
			
		case VChan_Closed:
			
			// undim number of samples controls and duration
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_NSamples, ATTR_DIMMED, 0);
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_Duration, ATTR_DIMMED, 0);
			break;
	}
}

static void	ADNSamplesSourceVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR

	ADTaskSet_type* 	tskSet			= VChanOwner; 
	uInt64*				nSamplesPtr		= NULL;
	DataPacket_type*	dataPacket		= NULL;
	
	switch (state) {
			
		case VChan_Open:
			
			// send number of samples if task is finite
			if (tskSet->timing->measMode != Operation_Finite) return;
	
			nullChk( nSamplesPtr = malloc(sizeof(uInt64)) );
			*nSamplesPtr = tskSet->timing->nSamples;
			nullChk( dataPacket = init_DataPacket_type(DL_UInt64, (void**) &nSamplesPtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
			break;
			
		case VChan_Closed:
			
			break;
	}
	
	return;
	
Error:
	
	OKfree(nSamplesPtr);
	discard_DataPacket_type(&dataPacket);
	
PRINT_ERR
}

static void ADSamplingRateSinkVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
	ADTaskSet_type* 	tskSet			= VChanOwner;
	
	switch (state) {
			
		case VChan_Open:
			// dim sampling rate control
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_SamplingRate, ATTR_DIMMED, 1);
			break;
			
		case VChan_Closed:
			// undim sampling rate control
			SetCtrlAttribute(tskSet->timing->settingsPanHndl, Set_SamplingRate, ATTR_DIMMED, 0);
			break;
	}
	
}

static void	ADSamplingRateSourceVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR

	ADTaskSet_type* 	tskSet			= VChanOwner;
	double*				samplingRatePtr	= NULL;
	DataPacket_type*	dataPacket		= NULL;
	
	switch (state) {
			
		case VChan_Open:
			
			nullChk( samplingRatePtr = malloc(sizeof(double)) );
			*samplingRatePtr = tskSet->timing->sampleRate;
			nullChk( dataPacket = init_DataPacket_type(DL_Double, (void**) &samplingRatePtr, NULL, NULL) );
			errChk( SendDataPacket(tskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
			break;
			
		case VChan_Closed:
			
			break;
	}
	
	return;
	
Error:
	
	OKfree(samplingRatePtr);
	discard_DataPacket_type(&dataPacket);
	
PRINT_ERR
}

static void COPulseTrainSinkVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
	ChanSet_CO_type* 		COChanSet		= GetVChanOwner(self);
	int						timingPanHndl	= 0;
	
	// dim timing UI pulse train controls
	GetPanelHandleFromTabPage(COChanSet->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl);
	
	switch (state) {
			
		case VChan_Open:
			
			SetPanelAttribute(timingPanHndl, ATTR_DIMMED, 1);
			break;
			
		case VChan_Closed:
			
			SetPanelAttribute(timingPanHndl, ATTR_DIMMED, 0);
			break;
	}
	
}

// pulsetrain command VChan connected callback
static void	COPulseTrainSourceVChan_StateChange (VChan_type* self, void* VChanOwner, VChanStates state)
{
INIT_ERR

	// construct pulse train packet when a connection is created with default settings and send it
	ChanSet_CO_type* 		COChanSet		= GetVChanOwner(self);	  
	PulseTrain_type* 		pulseTrain		= NULL;
	DataPacket_type*  		dataPacket		= NULL;
	PulseTrainTimingTypes 	pulseType		= 0; 
	
	switch (state) {
			
		case VChan_Open:
			
			nullChk ( pulseTrain = CopyPulseTrain(COChanSet->pulseTrain) );
			pulseType = GetPulseTrainType(pulseTrain); 
	
			switch(pulseType){
			
				case PulseTrain_Freq:
					nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, (void**) &pulseTrain, NULL,(DiscardFptr_type) discard_PulseTrain_type) );  
					break;
			
				case PulseTrain_Time:
					nullChk ( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, (void**) &pulseTrain, NULL,(DiscardFptr_type) discard_PulseTrain_type) );  
					break;
			
				case PulseTrain_Ticks: 
					nullChk ( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, (void**) &pulseTrain, NULL,(DiscardFptr_type) discard_PulseTrain_type) );  
					break;
			
			}
	
			// send data packet with pulsetrain
			errChk( SendDataPacket((SourceVChan_type*)self, &dataPacket, 0, &errorInfo.errMsg) );
			break;
			
		case VChan_Closed:
			
			break;
	}
	
	return;
	
Error:
	
	// cleanup
	discard_PulseTrain_type(&pulseTrain);
	discard_DataPacket_type(&dataPacket);
	discard_DataPacket_type(&dataPacket);
	
PRINT_ERR
}

/// HIFN Updates number of samples acquired when task controller is not active for analog and digital tasks.
static int ADNSamples_DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	// update only if task controller is not active
	if (taskActive) return 0;
	
	ADTaskSet_type*			tskSet			= GetVChanOwner((VChan_type*) sinkVChan);
	DataPacket_type*		dataPacket		= NULL;
	void*					dataPacketData	= NULL;
	DLDataTypes				dataPacketType;
	uInt64*					nSamplesPtr		= NULL;   
	
	if (tskSet->timing->measMode == Operation_Finite) {
		errChk( GetDataPacket(tskSet->timing->nSamplesSinkVChan, &dataPacket, &errorInfo.errMsg) );
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
				
			case DL_UInt64:
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
		dataPacket = init_DataPacket_type(DL_UInt64, (void**) &nSamplesPtr, NULL, NULL);
		errChk(SendDataPacket(tskSet->timing->nSamplesSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg));
	
	} else
		// n Samples cannot be used for continuous tasks, empty Sink VChan if there are any elements
		ReleaseAllDataPackets(tskSet->timing->nSamplesSinkVChan, NULL);
	
	
	return 0;

DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	ReleaseDataPacket(&dataPacket);
	OKfree(nSamplesPtr);

RETURN_ERR
}

/// HIFN Receives sampling rate info for analog and digital IO DAQmx tasks when task controller is not active.
static int ADSamplingRate_DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	// update only if task controller is not active
	if (taskActive) return 0;
	
	ADTaskSet_type*			tskSet				= GetVChanOwner((VChan_type*)sinkVChan);
	DataPacket_type*		dataPacket			= NULL;
	void*					dataPacketData		= NULL;
	DLDataTypes				dataPacketType		= 0;
	double*					samplingRatePtr		= NULL;
	
	// get data packet
	errChk( GetDataPacket(sinkVChan, &dataPacket, &errorInfo.errMsg) );
	dataPacketData = GetDataPacketPtrToData(dataPacket, &dataPacketType);
	
	switch (dataPacketType) {
		case DL_Float:
			tskSet->timing->sampleRate = (double)**(float**)dataPacketData;
			break;
				
		case DL_Double:
			tskSet->timing->sampleRate = **(double**)dataPacketData;
			break;
	}
	
	// if AI task, update oversampling if it must be adjusted automatically
	if (tskSet->readAIData && tskSet->timing->oversamplingAuto) {
		tskSet->timing->oversampling = (uInt32)(tskSet->timing->targetSampleRate/tskSet->timing->sampleRate);
		if (!tskSet->timing->oversampling) tskSet->timing->oversampling = 1;
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Oversampling, tskSet->timing->oversampling);
				
		// update also read AI data structures
		discard_ReadAIData_type(&tskSet->readAIData);
		tskSet->readAIData = init_ReadAIData_type(tskSet->dev);
	}
	
	// if AI task
	if (tskSet->readAIData) {
		// given oversampling, adjust also the actual DAQ sampling rate display
		SetCtrlVal(tskSet->timing->settingsPanHndl, Set_ActualSamplingRate, tskSet->timing->sampleRate * tskSet->timing->oversampling * 1e-3);	// display in [kHz]
		// update sampling rate in dev structure
		DAQmxErrChk( DAQmxSetTimingAttribute(tskSet->taskHndl, DAQmx_SampClk_Rate, tskSet->timing->sampleRate * tskSet->timing->oversampling) );
	} else {
		// update sampling rate in dev structure
		DAQmxErrChk( DAQmxSetTimingAttribute(tskSet->taskHndl, DAQmx_SampClk_Rate, tskSet->timing->sampleRate) );
		
	}
	
	// update sampling rate in UI
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_SamplingRate, tskSet->timing->sampleRate/1000);	// display in [kHz]
	// update duration in UI
	SetCtrlVal(tskSet->timing->settingsPanHndl, Set_Duration, tskSet->timing->nSamples / tskSet->timing->sampleRate);
	
	
	
	// cleanup
	ReleaseDataPacket(&dataPacket);
	
	// send data
	nullChk( samplingRatePtr = malloc(sizeof(double)) );
	*samplingRatePtr = tskSet->timing->sampleRate;
	dataPacket = init_DataPacket_type(DL_Double, (void**) &samplingRatePtr, NULL, NULL);
	errChk(SendDataPacket(tskSet->timing->samplingRateSourceVChan, &dataPacket, FALSE, &errorInfo.errMsg));
	
	return 0;

DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	ReleaseDataPacket(&dataPacket);
	OKfree(samplingRatePtr);
	
RETURN_ERR
}

static int AO_DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	Dev_type*						dev					= GetTaskControlModuleData(taskControl);
	ChanSet_type*					chan				= GetVChanOwner((VChan_type*)sinkVChan);
	DataPacket_type**				dataPackets			= NULL;
	double** 						doubleDataPtrPtr	= NULL;
	float**							floatDataPtrPtr		= NULL;
	size_t							nPackets			= 0;
	size_t							nElem				= 0;
	TaskHandle						taskHndl			= 0;
	ChanSet_AO_Voltage_type*		aoVoltageChan		= NULL;
	ChanSet_AO_Current_type*		aoCurrentChan		= NULL;
	DLDataTypes						dataPacketType		= 0;
	void*							dataPacketDataPtr	= NULL;
	double							doubleData			= 0;
	
	// do not update voltage if task controller is active and not on demand
	if (taskActive && !chan->onDemand) return 0;						  
	
	
	// set AO task to Verified state if it exists and is not on demand
	if (dev->AOTaskSet->taskHndl && !chan->onDemand)
		DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Abort) );
	
	
	
	// get all available data packets
	errChk( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errorInfo.errMsg) );
				
	// create DAQmx task and channel
	switch (chan->chanType) {
						
		case Chan_AO_Voltage:
			aoVoltageChan = (ChanSet_AO_Voltage_type*) chan;				
			// create task
			DAQmxErrChk(DAQmxCreateTask("On demand AO voltage task", &taskHndl));
			// create DAQmx channel  
			DAQmxErrChk(DAQmxCreateAOVoltageChan(taskHndl, chan->name, "", aoVoltageChan->VMin , aoVoltageChan->VMax, DAQmx_Val_Volts, ""));  
			break;
							
		case Chan_AO_Current:
			aoCurrentChan = (ChanSet_AO_Current_type*) chan;				
			// create task
			DAQmxErrChk(DAQmxCreateTask("On demand AO current task", &taskHndl));
			// create DAQmx channel  
			DAQmxErrChk(DAQmxCreateAOCurrentChan(taskHndl, chan->name, "", aoCurrentChan->IMin , aoCurrentChan->IMax, DAQmx_Val_Amps, ""));  
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
						doubleDataPtrPtr = (double**) GetWaveformPtrToData(*(Waveform_type**)dataPacketDataPtr, &nElem);
						for (size_t j = 0; j < nElem; j++) {							  // update AO one sample at a time from the provided waveform
							DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &(*doubleDataPtrPtr)[j], NULL, NULL));
							if (*abortFlag) break;
						}
						break;
						
					case DL_Waveform_Float:
						floatDataPtrPtr = (float**) GetWaveformPtrToData(*(Waveform_type**)dataPacketDataPtr, &nElem);
						for (size_t j = 0; j < nElem; j++) {							  // update AO one sample at a time from the provided waveform
							doubleData = (double) (*floatDataPtrPtr)[j];
							DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &doubleData, NULL, NULL));
							if (*abortFlag) break;
						}
						break;
						
					case DL_RepeatedWaveform_Double:
						doubleDataPtrPtr = (double**) GetRepeatedWaveformPtrToData(*(RepeatedWaveform_type**)dataPacketDataPtr, &nElem);
						// repeat only once
						for (size_t j = 0; j < nElem; j++) {							  // update AO one sample at a time from the provided waveform
							DAQmxErrChk(DAQmxWriteAnalogF64(taskHndl, 1, 1, dev->AOTaskSet->timeout, DAQmx_Val_GroupByChannel, &(*doubleDataPtrPtr)[j], NULL, NULL));
							if (*abortFlag) break;
						}
						break;
						
					case DL_RepeatedWaveform_Float:
						floatDataPtrPtr = (float**) GetRepeatedWaveformPtrToData(*(RepeatedWaveform_type**)dataPacketDataPtr, &nElem);
						// repeat only once
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
						
				}
						
			}
			break;
	}
	
	DAQmxErrChk(DAQmxClearTask(taskHndl));
	
	// set AO task to Commited state if it exists and is not on demand
	if (dev->AOTaskSet->taskHndl && !chan->onDemand) {
		DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Reserve) );
		DAQmxErrChk( DAQmxTaskControl(dev->AOTaskSet->taskHndl, DAQmx_Val_Task_Commit) );  
	}
	
	return 0;
	
DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	for (size_t i = 0; i < nPackets; i++)
		ReleaseDataPacket(&dataPackets[i]); 
	OKfree(dataPackets);
	
	DAQmxClearTask(taskHndl);
	
RETURN_ERR
}

static int DO_DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
	return 0;
}

static int CO_DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR
	
	DataPacket_type**		dataPackets			= NULL;
	DataPacket_type*		dataPacket			= NULL;
	size_t					nPackets			= 0;
	ChanSet_CO_type*		chanSetCO			= GetVChanOwner((VChan_type*)sinkVChan);	
	DLDataTypes				dataPacketType		= 0;  
	PulseTrain_type*    	pulseTrain			= NULL;
	
	// update only if task controller is not active
	if (taskActive) return 0;
	
	// get all available data packets and use only the last packet
	errChk ( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errorInfo.errMsg) );
			
	for (size_t i = 0; i < nPackets-1; i++) 
		ReleaseDataPacket(&dataPackets[i]);  
				
	pulseTrain = *(PulseTrain_type**) GetDataPacketPtrToData(dataPackets[nPackets-1], &dataPacketType);
	discard_PulseTrain_type(&chanSetCO->pulseTrain); 
	nullChk( chanSetCO->pulseTrain = CopyPulseTrain(pulseTrain) );
	ReleaseDataPacket(&dataPackets[nPackets-1]); 
	
	// get channel panel handle to adjust UI settings
	int						timingPanHndl;
		
	GetPanelHandleFromTabPage(chanSetCO->baseClass.chanPanHndl, CICOChSet_TAB, DAQmxCICOTskSet_TimingTabIdx, &timingPanHndl); 
		
	double 					initialDelay		= 0;
	double					frequency			= 0;
	double					dutyCycle			= 0;
	double					lowTime				= 0;
	double					highTime			= 0;
	uInt32					initialDelayTicks	= 0;
	uInt32					lowTicks			= 0;
	uInt32					highTicks			= 0;
	PulseTrainIdleStates	idleState			= 0;
	PulseTrainModes			pulseMode			= 0;
	uInt64					nPulses				= 0;
	int						ctrlIdx				= 0;
		
	// set idle state
	idleState = GetPulseTrainIdleState(chanSetCO->pulseTrain);
	DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_IdleState, PulseTrainIdleStates_To_DAQmxVal(idleState)) );
	// generation mode
	pulseMode = GetPulseTrainMode(chanSetCO->pulseTrain);
	// timing
	nPulses = GetPulseTrainNPulses(chanSetCO->pulseTrain);
	DAQmxErrChk ( DAQmxCfgImplicitTiming(chanSetCO->taskHndl, PulseTrainModes_To_DAQmxVal(pulseMode), nPulses) );
		
	switch (dataPacketType) {
		case DL_PulseTrain_Freq:
			// set initial delay
			initialDelay = GetPulseTrainFreqTimingInitialDelay((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq_InitialDelay, initialDelay) );
			SetCtrlVal(timingPanHndl, COFreqPan_InitialDelay, initialDelay); 
			// frequency
			frequency = GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Freq, frequency) );
			SetCtrlVal(timingPanHndl, COFreqPan_Frequency, frequency); 
			// duty cycle
			dutyCycle = GetPulseTrainFreqTimingDutyCycle((PulseTrainFreqTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_DutyCyc, dutyCycle/100) ); // normalize from [%] to 0-1 range
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
			GetIndexFromValue(timingPanHndl, COFreqPan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(pulseMode)); 
			SetCtrlIndex(timingPanHndl, COFreqPan_Mode, ctrlIdx);
			break;
				
		case DL_PulseTrain_Time:
			// set initial delay
			initialDelay = GetPulseTrainTimeTimingInitialDelay((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Time_InitialDelay, initialDelay) );
			SetCtrlVal(timingPanHndl, COTimePan_InitialDelay, initialDelay); 
			// low time
			lowTime = GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTime, lowTime) );
			SetCtrlVal(timingPanHndl, COTimePan_LowTime, lowTime); 
			// high time
			highTime = GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTime, highTime) );
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
			GetIndexFromValue(timingPanHndl, COTimePan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(pulseMode)); 
			SetCtrlIndex(timingPanHndl, COTimePan_Mode, ctrlIdx);
			break;
			
		case DL_PulseTrain_Ticks:
			// tick delay
			initialDelayTicks = GetPulseTrainTickTimingDelayTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_Ticks_InitialDelay, initialDelayTicks) );
			SetCtrlVal(timingPanHndl, COTicksPan_InitialDelay, initialDelayTicks); 
			// low ticks
			lowTicks = GetPulseTrainTickTimingLowTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_LowTicks, lowTicks) );
			SetCtrlVal(timingPanHndl, COTicksPan_LowTicks, lowTicks); 
			// high ticks
			highTicks = GetPulseTrainTickTimingHighTicks((PulseTrainTickTiming_type*)chanSetCO->pulseTrain);
			DAQmxErrChk( DAQmxSetChanAttribute(chanSetCO->taskHndl, chanSetCO->baseClass.name, DAQmx_CO_Pulse_HighTicks, highTicks) );
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
			GetIndexFromValue(timingPanHndl, COTicksPan_Mode, &ctrlIdx, PulseTrainModes_To_DAQmxVal(pulseMode)); 
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
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Freq, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) );  
			break;
			
		case PulseTrain_Time:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Time, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) ); 
			break;
			
		case PulseTrain_Ticks:
			nullChk( dataPacket = init_DataPacket_type(DL_PulseTrain_Ticks, (void**) &pulseTrain, NULL, (DiscardFptr_type) discard_PulseTrain_type) ); 
			break;
		
	}
	
	errChk( SendDataPacket(chanSetCO->baseClass.srcVChan, &dataPacket, FALSE, &errorInfo.errMsg) );
						
	goto Error;

DAQmxError:
	
DAQmx_ERROR_INFO

Error:
	
	// cleanup
	discard_DataPacket_type(&dataPacket);
	OKfree(dataPackets);				
	
RETURN_ERR
}

//---------------------------------------------------------------------------------------------------------------------------------------
// NIDAQmx Device Task Controller Callbacks
//---------------------------------------------------------------------------------------------------------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	Dev_type*		dev				= GetTaskControlModuleData(taskControl);
	BOOL			continuousFlag 	= FALSE;
	double			waitTime 		= 0;
	size_t 			nRepeat 		= 0;
	
	// set finite/continuous Mode and dim number of repeats if neccessary
	GetCtrlVal(dev->devPanHndl, TaskSetPan_Mode, &continuousFlag);
	if (continuousFlag) {
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, 1);
		SetTaskControlMode(dev->taskController, TASK_CONTINUOUS);
	} else {
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, 0);
		SetTaskControlMode(dev->taskController, TASK_FINITE);
	}
	
	// wait time
	GetCtrlVal(dev->devPanHndl, TaskSetPan_Wait, &waitTime);
	SetTaskControlIterationsWait(dev->taskController, waitTime);
	
	// repeats
	GetCtrlVal(dev->devPanHndl, TaskSetPan_Repeat, &nRepeat);
	SetTaskControlIterations(dev->taskController, nRepeat);
	
	// reset current iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, 0);
	
	// configure device again
	errChk( ConfigDAQmxDevice(dev, errorInfo.errMsg) );
	
Error:
	
RETURN_ERR
}

static int UnconfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//Dev_type*	dev	= GetTaskControlModuleData(taskControl); 
	
	return 0; 
}

static void	IterateTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag)
{
INIT_ERR

	Dev_type*		dev											= GetTaskControlModuleData(taskControl);
	char			CmtErrMsgBuffer[CMT_MAX_MESSAGE_BUF_SIZE]	= "";
	int  			nActiveTasks								= 0;
	int*			nActiveTasksPtr								= &nActiveTasks;	// Keeps track of the number of DAQmx tasks that must still complete before the iteration is considered to be complete
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, GetCurrentIterIndex(iterator));
	
	//----------------------------------------
	// Launch and count active tasks 
	//----------------------------------------
	
	// get active tasks counter handle
	CmtErrChk( CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr) );
	nActiveTasks = *nActiveTasksPtr;
	
	// AI
	if (dev->AITaskSet && dev->AITaskSet->taskHndl && dev->AITaskSet->nOpenChannels) {
		CmtErrChk( CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartAIDAQmxTask_CB, dev, NULL) );
		nActiveTasks++;
	}
	
	// AO
	if (dev->AOTaskSet && dev->AOTaskSet->taskHndl && dev->AOTaskSet->nOpenChannels) {
		//test lex
		//quick and dirty workaround;
		//for some reason the AODAQmxTaskDone_CB callback gets 'lost'after an iteration
		//just re-registering it here
		DAQmxErrChk( DAQmxRegisterDoneEvent(dev->AOTaskSet->taskHndl, 0, NULL, dev) );     
		DAQmxErrChk( DAQmxRegisterDoneEvent(dev->AOTaskSet->taskHndl, 0, AODAQmxTaskDone_CB, dev) );  
		
		CmtErrChk( CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartAODAQmxTask_CB, dev, NULL) );
		nActiveTasks++;
	}
	
	// DI
	if (dev->DITaskSet && dev->DITaskSet->taskHndl)
		nActiveTasks++;
	
	// DO
	if (dev->DOTaskSet && dev->DOTaskSet->taskHndl)
		nActiveTasks++;
	
	// CI
	if (dev->CITaskSet) {
		ChanSet_CI_type* 	chanSet		= NULL;
		size_t 				nCI 		= ListNumItems(dev->CITaskSet->chanTaskSet);
		for (size_t i = 1; i <= nCI; i++) {
			chanSet = *(ChanSet_CI_type**)ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			if (chanSet->taskHndl) {
				CmtErrChk( CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartCIDAQmxTasks_CB, chanSet, NULL) );
				nActiveTasks++;
			}
		}
	}
	
	// CO
	if (dev->COTaskSet) {
		ChanSet_CO_type*	chanSet		= NULL;
		size_t 				nCO 		= ListNumItems(dev->COTaskSet->chanTaskSet);
		for (size_t i = 1; i <= nCO; i++) {
			chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
			if (chanSet->taskHndl) {
				CmtErrChk( CmtScheduleThreadPoolFunction(DEFAULT_THREAD_POOL_HANDLE, StartCODAQmxTasks_CB, chanSet, NULL) );
				nActiveTasks++;
			}
		}
	}
	
	CmtErrChk( CmtSetTSV(dev->nActiveTasks, &nActiveTasks) );
	// release active tasks counter handle
	CmtErrChk( CmtReleaseTSVPtr(dev->nActiveTasks) );
	
	return; // no error
	
DAQmxError:
	
DAQmx_ERROR_INFO

CmtError:
	
Cmt_ERR

Error:
	
	errChk( CmtReleaseTSVPtr(dev->nActiveTasks) );
	AbortTaskControlExecution(taskControl);
	OKfree(errorInfo.errMsg);
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	//Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static int DoneTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations,GetCurrentIterIndex(iterator));
	
	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	Dev_type*	dev		= GetTaskControlModuleData(taskControl);
	
	// update iteration display
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, GetCurrentIterIndex(iterator));
	
	return 0;
}

static int IterationStopTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR
	
	Dev_type*	dev					= GetTaskControlModuleData(taskControl);
	int*		nActiveTasksPtr		= NULL;
	bool32		taskDoneFlag		= FALSE;
	
	
	CmtErrChk( CmtGetTSVPtr(dev->nActiveTasks, &nActiveTasksPtr) );
	
	// stop CO tasks explicitely
	if (dev->COTaskSet) {
		ChanSet_CO_type* 	chanSet	= NULL;
		size_t				nChans	= ListNumItems(dev->COTaskSet->chanTaskSet);
	   	for (size_t i = 1; i <= nChans; i++) {	
			chanSet = *(ChanSet_CO_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);	
			if (chanSet->taskHndl) {
				DAQmxErrChk( DAQmxIsTaskDone(chanSet->taskHndl, &taskDoneFlag) ); 
				if (!taskDoneFlag ) {
					DAQmxErrChk( DAQmxStopTask(chanSet->taskHndl) );
					(*nActiveTasksPtr)--;
				}
			}
		}
	}
	
	// check if iteration can be set to done
	if (!*nActiveTasksPtr)
		errChk( TaskControlIterationDone(dev->taskController, 0, "", FALSE, &errorInfo.errMsg) );
		
	CmtErrChk( CmtReleaseTSVPtr(dev->nActiveTasks) );
	
	return 0;

DAQmxError:
	
DAQmx_ERROR_INFO

CmtError:
	
Cmt_ERR

Error:
	
RETURN_ERR
}

static int TaskTreeStateChange (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
INIT_ERR

	Dev_type*			dev			= GetTaskControlModuleData(taskControl);
	ChanSet_type*		chanSet		= NULL;
	size_t				nChans		= 0;	
	int					panHndl		= 0;
	
	// device panel
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IO, ATTR_DIMMED, (int) state);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IOMode, ATTR_DIMMED, (int) state);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_IOType, ATTR_DIMMED, (int) state);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_PhysChan, ATTR_DIMMED, (int) state);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Mode, ATTR_DIMMED, (int) state);
	if (GetTaskControlMode(dev->taskController) == TASK_FINITE)
		SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Repeat, ATTR_DIMMED, (int) state);
	SetCtrlAttribute(dev->devPanHndl, TaskSetPan_Wait, ATTR_DIMMED, (int) state);
	// AI
	if (dev->AITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->AITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AITaskSet->chanSet, i);
			SetPanelAttribute(chanSet->chanPanHndl, ATTR_DIMMED, (int) state); 
		}
		// settings panel
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_SettingsTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) state);
		// timing panel
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TimingTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) state);
		// trigger panel
		int 	trigPanHndl;
		int		nTabs;
		GetPanelHandleFromTabPage(dev->AITaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TriggerTabIdx, &trigPanHndl);
		GetNumTabPages(trigPanHndl, Trig_TrigSet, &nTabs);
		for (size_t i = 0; i < nTabs; i++) {
			GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, i, &panHndl);
			SetPanelAttribute(panHndl, ATTR_DIMMED, (int) state);
		}
	}
	// AO
	if (dev->AOTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->AOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->AOTaskSet->chanSet, i);
			SetPanelAttribute(chanSet->chanPanHndl, ATTR_DIMMED, (int) state); 
		}
		// settings panel
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_SettingsTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) state);
		// timing panel
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TimingTabIdx, &panHndl);
		SetPanelAttribute(panHndl, ATTR_DIMMED, (int) state);
		// trigger panel
		int 	trigPanHndl;
		int		nTabs;
		GetPanelHandleFromTabPage(dev->AOTaskSet->panHndl, ADTskSet_Tab, DAQmxADTskSet_TriggerTabIdx, &trigPanHndl);
		GetNumTabPages(trigPanHndl, Trig_TrigSet, &nTabs);
		for (size_t i = 0; i < nTabs; i++) {
			GetPanelHandleFromTabPage(trigPanHndl, Trig_TrigSet, i, &panHndl);
			SetPanelAttribute(panHndl, ATTR_DIMMED, (int) state);
		}
	}
	// DI
	if (dev->DITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->DITaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DITaskSet->chanSet, i);
			SetPanelAttribute(chanSet->chanPanHndl, ATTR_DIMMED, (int) state); 
		}
		
		// dim/undim rest of panels
	}
	// DO
	if (dev->DOTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->DOTaskSet->chanSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->DOTaskSet->chanSet, i);
			SetPanelAttribute(chanSet->chanPanHndl, ATTR_DIMMED, (int) state); 
		}
		
		// dim/undim rest of panels
	}
	// CI
	if (dev->CITaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->CITaskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->CITaskSet->chanTaskSet, i);
			SetPanelAttribute(chanSet->chanPanHndl, ATTR_DIMMED, (int) state); 
		}
		
		// dim/undim rest of panels
	}
	// CO
	if (dev->COTaskSet) {
		// channel panels 
		nChans = ListNumItems(dev->COTaskSet->chanTaskSet);
		for (size_t i = 1; i <= nChans; i++) {
			chanSet = *(ChanSet_type**)ListGetPtrToItem(dev->COTaskSet->chanTaskSet, i);
			SetPanelAttribute(chanSet->chanPanHndl, ATTR_DIMMED, (int) state); 
		}
		
		// dim/undim rest of panels
	}
	
	// just make sure the device is configured again before start
	if (state == TaskTree_Started)
		errChk( ConfigDAQmxDevice(dev, &errorInfo.errMsg) );
	
Error:
	
RETURN_ERR
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	SetCtrlVal(dev->devPanHndl, TaskSetPan_TotalIterations, 0);
	
	return 0;
}

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char errorMsg[])
{
	Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	StopDAQmxTasks(dev, NULL);
}

static int ModuleEventHandler (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg)
{
	//Dev_type*	dev	= GetTaskControlModuleData(taskControl);
	
	return 0;
}
 
