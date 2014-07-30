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
#include <userint.h>
#include "DAQLabModule.h"
#include "NIDAQmxManager.h"
#include "UI_NIDAQmxManager.h"
#include <nidaqmx.h>

//========================================================================================================================================================================================================
// Constants

	// NIDAQmxManager UI resource 
#define MOD_NIDAQmxManager_UI 		"./Modules/NIDAQmxManager/UI_NIDAQmxManager.uir"	
	// max number of DAQmx tasks that can be configured on the device.
#define MAX_DAQmx_TASKS 6
	// Taskset constants
#define	AITSK			0
#define	AOTSK			1  
#define	DITSK			2
#define	DOTSK			3	  
#define	CITSK			4
#define	COTSK			5  
	// IO
#define DAQmxAcquire	0
#define DAQmxGenerate	1
#define DAQmxAnalog		0
#define DAQmxDigital	1
#define DAQmxCounter	2
#define DAQmxTEDS		3   // not used
#define DAQmxDigLines	0
#define DAQmxDigPorts	1

//========================================================================================================================================================================================================
// Types

// Ranges for DAQmx IO, stored as pairs of low value and high value, e.g. -10 V, 10 V or -1 mA, 1 mA
typedef struct {
	int 				Nrngs;   					// number of ranges
	double* 			low;  						// low value 
	double* 			high; 						// high value
} IORange_type;

// DAQmx Channel definitions

// AI
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/ai0
	ListType			supportedMeasTypes;			// List of int type. Indicates the measurement types supported by the channel.
	int					terminalCfg;				// List of bitfield terminal configurations supported by the channel.
	IORange_type* 		Vrngs;      				// Ranges for voltage input  [V]
	IORange_type* 		Irngs;      				// Ranges for current input  [A]
} DAQmxAIChannel_type;

// AO
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/ao0
	ListType			supportedOutputTypes;		// List of int type. Indicates the output types supported by the channel.
	int					terminalCfg;				// List of bitfield terminal configurations supported by the channel.
	IORange_type* 		Vrngs;      				// Ranges for voltage output  [V]
	IORange_type* 		Irngs;      				// Ranges for current output  [A]
} DAQmxAOChannel_type;

// DI Line
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/port0/line0
	ListType			sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital input.
	BOOL				sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital input physical channel.
	BOOL				changeDetectSupported;		// Indicates if the change detection timing type is supported for the digital input physical channel.
} DAQmxDILineChannel_type;

// DI Port
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/port0
	ListType			sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital input.
	BOOL				sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital input physical channel.
	BOOL				changeDetectSupported;		// Indicates if the change detection timing type is supported for the digital input physical channel.
	unsigned int		portWidth;					// Indicates in bits the width of digital input port.
} DAQmxDIPortChannel_type;

// DO Line
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/port0/line0
	BOOL				sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital output physical channel.
	ListType			sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital output.
} DAQmxDOLineChannel_type;

// DO Port
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/port0
	BOOL				sampClkSupported;			// Indicates if the sample clock timing type is supported for the digital output physical channel.
	ListType			sampModes;					// List of int type. Indicates the sample modes supported by devices that support sample clocked digital output.
	unsigned int		portWidth;					// Indicates in bits the width of digital output port.
} DAQmxDOPortChannel_type;

// CI
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/ctr0
	ListType			supportedMeasTypes;			// Indicates the measurement types supported by the channel.
} DAQmxCIChannel_type;

// CO
typedef struct {
	char* 				physChanName;				// Physical channel name, e.g. dev1/ctr0
	ListType			supportedOutputTypes;		// Indicates the output types supported by the channel.
} DAQmxCOChannel_type;

// DAQ device parameters
typedef struct {
	char*        		name;    	   				// Device name.
	char*        		type;    	   				// Indicates the product name of the device.
	unsigned int		productNum;					// unique product number
	unsigned int		serial;       				// device serial number
	double       		AISCmaxrate;  				// Single channel AI max rate   [Hz]
	double       		AIMCmaxrate;  				// Multiple channel AI max rate [Hz]
	double       		AIminrate;    				// AI minimum rate              [Hz]
	double       		AOmaxrate;    				// AO maximum rate              [Hz]
	double       		AOminrate;    				// AO minimum rate              [Hz]
	double       		DImaxrate;    				// DI maximum rate              [Hz]
	double       		DOmaxrate;    				// DO maximum rate              [Hz]
	ListType     		AIchan;       				// List of DAQmxAIChannel_type*
	ListType     		AOchan;	   					// List of DAQmxAOChannel_type*
	ListType     		DIlines;      				// List of DAQmxDILineChannel_type*
	ListType     		DIports;      				// List of DAQmxDIPortChannel_type*
	ListType     		DOlines;      				// List of DAQmxDOLineChannel_type*
	ListType     		DOports;      				// List of DAQmxDOPortChannel_type*
	ListType     		CIchan;	   					// List of DAQmxCIChannel_type*
	ListType     		COchan;	   					// List of DAQmxCOChannel_type*
	BOOL				AnalogTriggering;			// Indicates if the device supports analog triggering.
	BOOL				DigitalTriggering;			// Indicates if the device supports digital triggering.
	ListType			AISupportedMeasTypes;		// List of int type. Indicates the measurement types supported by the physical channels of the device for analog input.
	ListType			AOSupportedOutputTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for analog output.
	ListType			CISupportedMeasTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for counter input.
	ListType			COSupportedOutputTypes;		// List of int type. Indicates the generation types supported by the physical channels of the device for counter output.
} DAQmxDevAttr_type;

// Terminal type
typedef enum{
	T_NONE,      									// no terminal 
	T_DIFF,      									// differential 
	T_RSE,       									// referenced single-ended 
	T_NRSE,      									// non-referenced single-ended
	T_PSEUDODIFF 									// pseudo-differential
} Terminal_type;

typedef enum{
	TRIG_None,
	TRIG_DigitalEdge,
	TRIG_DigitalPattern,
	TRIG_AnalogEdge,
	TRIG_AnalogWindow,
} Trig_type;

typedef struct {
	Trig_type 			trigtype;
	char*     			trigsource;   				// dynamically allocated!
	BOOL      			edgetype;     				// 0 = Rising, 1 = Falling, for analog and digital edge trig. type
	double   			level; 						// for analog edge trig. type
	double    			windowtop;    				// only for analog window trig type
	double   			windowbttm;   				// only for analog window trig. type
	BOOL      			wndtrigcond;  				// only for analog window trig type, 0 = Entering window, 1 = Leaving window
} TaskTrig_type;

// Analog Input & Output channel type settings
typedef struct {
	double        		Vmax;      					// maximum voltage [V]
	double        		Vmin;       				// minimum voltage [V]
	Terminal_type 		terminal;   				// terminal type 	
} AIAOChanSet_type;

// Digital Line/Port Input & Digital Line/Port Output channel type settings
typedef struct {
	BOOL 				invert;    					// invert polarity (for ports the polarity is inverted for all lines)
} DIDOChanSet_type;

// Counter Input for Counting Edges, channel type settings
typedef struct {
	BOOL 				ActiveEdge; 				// 0 - Falling, 1 - Rising
	int 				initialcount;				// value to start counting from
	enum {
		C_UP, 
		C_DOWN, 
		C_EXT
	} 					cdirection; 				// count direction, C_EXT = externally controlled
} CIEdgeChanSet_type;

// Counter Output channel type settings
typedef struct {
	double 				hightime;   				// the time the pulse stays high [s]
	double 				lowtime;    				// the time the pulse stays low [s]
	double 				initialdel; 				// initial delay [s]
	BOOL 				idlestate;    				// LOW = 0, HIGH = 1 - terminal state at rest, pulses switch this to the oposite state
} COChanSet_type;
	
// channel settings wrap structure
typedef struct {
	char*				name;						// dynamically allocated full physical channel name used by DAQmx, e.g. Dev1/ai1 
	char*				vname;       				// dynamically allocated virtual channel name, e.g. GalvoX
	void*				settings;    				// points to one of the specific channel settings structures  such as AIAOChanSet_type etc..
} IOchanSet_type;

// Channels used in DAQ task
typedef struct {
	int      			nAI;           				// # AI chan
	int      			nAO;           				// # AO chan
	int      			nDILines;      				// # DI lines
	int      			nDIPorts;      				// # DI ports
	int      			nDOLines;      				// # DO lines
	int      			nDOPorts;      				// # DO ports
	int      			nCI;       					// # Counter inputs
	int      			nCO;       					// # Counter outputs
	ListType 			AIchanSet;     				// list of IOchanSet_type for AI channels configured for the DAQ task
	ListType 			AOchanSet;     				// list of IOchanSet_type for AO channels configured for the DAQ task 
	ListType 			DIlinesSet;    				// list of IOchanSet_type for DIlines channels configured for the DAQ task
	ListType 			DIportsSet;    				// list of IOchanSet_type for DIports channels configured for the DAQ task
	ListType 			DOlinesSet;    				// list of IOchanSet_type for DOlines channels configured for the DAQ task
	ListType 			DOportsSet;    				// list of IOchanSet_type for DOports channels configured for the DAQ task
	ListType 			CIchanSet;	    			// list of IOchanSet_type for CIchan channels configured for the DAQ task
	ListType 			COchanSet;	    			// list of IOchanSet_type for COchan channels configured for the DAQ task
} IOchan_type;

// Measurement mode
typedef enum{
	MeasNone, // no selection
	MeasFinite,
	MeasCont,
	MeasContRegen
} MeasMode_type; 

// Settings for DAQ task
typedef struct {
	TaskHandle			taskHndl;					// DAqmx task handle.
	double        		timeout;       				// Task timeout [s]
	MeasMode_type 		measmode;      				// Measurement mode e.g. finite or continuous.
	double       		samplerate;    				// Sampling rate in [Hz].
	size_t        		nsamples;	    			// Total number of samples to be acquired in case of a finite recording.
	double        		duration;	    			// Duration of finite recording.
	size_t        		blocksize;     				// Number of samples for reading or writing after which callbacks are called.
	TaskTrig_type 		starttrig;     				// Task trigger data.
	
	char*         		sampclksource; 				// Sample clock source if NULL then OnboardClock is used, otherwise the given clock
	BOOL          		sampclkedge;   				// Sample clock active edge, 0 = Rising, 1 = Falling
	
	char*         		refclksource;  				// Reference clock source used to sync internal clock, if NULL internal clock has no reference clock 
	double        		refclkfreq;    				// Reference clock frequency if such a clock is used
} DAQTaskSet_type;

// DAQ Task definition
typedef struct {
	DAQmxDevAttr_type*	attr;						// Device to be used during IO task
	IOchan_type*    	chan;						// Channels used in the task of IOchan_type
	TaskControl_type*   taskController;				// Task Controller for the DAQmx module
	DAQTaskSet_type 	taskset[MAX_DAQmx_TASKS];	// Array of task settings of DAQTaskSet_type// task settings: [0] = AI, [1] = AO, [2] = DI, [3] = DO, [4] = CI, [5] = CO
} DAQmxDev_type;

// Used for continuous data AO streaming
typedef struct {
	size_t        		writeblock;        			// Size of writeblock is IO_block_size
	size_t        		numchan;           			// Number of output channels
	
	float64**     		datain;            
	float64**     		databuff;
	float64*      		dataout;					// Array length is writeblock * numchan used for DAQmx write call, data is grouped by channel
	CmtTSQHandle* 		queue;
	
	size_t*       		datain_size; 
	size_t*      		databuff_size;
	
	size_t*       		idx;						// Array of pointers with index for each channel from which to continue writing data (not used yet)  
	size_t*       		datain_repeat;	 			// Number of times to repeat the an entire data packet
	size_t*      		datain_remainder;  			// Number of elements from the beginning of the data packet to still generate, 
									 				// WARNING: this does not apply when looping is ON. In this case when receiving a new data packet, a full cycle is generated before switching.
	
	BOOL*         		datain_loop;	   			// Data will be looped regardless of repeat value until new data packet is provided
} WriteAOData;

// Used for continuous data AO streaming 
typedef struct {
	WriteAOData*   		writeAOdata;
} RawDAQData;

//========================================================================================================================================================================================================
// Module implementation

struct NIDAQmxManager {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 	baseClass;
	
	// DATA
	
	ListType            DAQmxDevices;   // list of DAQmxDev_type* for each DAQmx device 
	
		//-------------------------
		// UI
		//-------------------------
	
	int					mainPanHndl;
	int					taskSetPanHndl;
	int					devListPanHndl;
	
	
	// METHODS 
	
};

//========================================================================================================================================================================================================
// Static global variables

static int	 currDev = -1;        // currently selected device from the DAQ table

//========================================================================================================================================================================================================
// Static functions

static DAQmxDev_type* 				init_DAQmxDev_type 					(DAQmxDevAttr_type** attr, char taskControllerName[]);
static void 						discard_DAQmxDev_type				(DAQmxDev_type** a);

static int 							init_DevList 						(ListType devlist, int panHndl, int tableCtrl);
static void 						empty_DevList 						(ListType devList); 

	// device attributes management
static DAQmxDevAttr_type* 			init_DAQmxDevAttr_type				(void); 
static void 						discard_DAQmxDevAttr_type			(DAQmxDevAttr_type** a);
static DAQmxDevAttr_type* 			copy_DAQmxDevAttr_type				(DAQmxDevAttr_type* attr);

	// AI channel
static DAQmxAIChannel_type* 		init_DAQmxAIChannel_type			(void); 
static void 						discard_DAQmxAIChannel_type			(DAQmxAIChannel_type** a);
int CVICALLBACK 					DisposeDAQmxAIChannelList 			(size_t index, void *ptrToItem, void *callbackData);
static DAQmxAIChannel_type* 		copy_DAQmxAIChannel_type			(DAQmxAIChannel_type* channel);
static ListType						copy_DAQmxAIChannelList				(ListType chanList);

	// AO channel
static DAQmxAOChannel_type* 		init_DAQmxAOChannel_type			(void); 
static void 						discard_DAQmxAOChannel_type			(DAQmxAOChannel_type** a);
int CVICALLBACK 					DisposeDAQmxAOChannelList 			(size_t index, void *ptrToItem, void *callbackData);
static DAQmxAOChannel_type* 		copy_DAQmxAOChannel_type			(DAQmxAOChannel_type* channel);
static ListType						copy_DAQmxAOChannelList				(ListType chanList);

	// DI Line channel
static DAQmxDILineChannel_type* 	init_DAQmxDILineChannel_type		(void); 
static void 						discard_DAQmxDILineChannel_type		(DAQmxDILineChannel_type** a);
int CVICALLBACK 					DisposeDAQmxDILineChannelList 		(size_t index, void *ptrToItem, void *callbackData);
static DAQmxDILineChannel_type* 	copy_DAQmxDILineChannel_type		(DAQmxDILineChannel_type* channel);
static ListType						copy_DAQmxDILineChannelList			(ListType chanList);

	// DI Port channel
static DAQmxDIPortChannel_type* 	init_DAQmxDIPortChannel_type		(void); 
static void 						discard_DAQmxDIPortChannel_type		(DAQmxDIPortChannel_type** a);
int CVICALLBACK 					DisposeDAQmxDIPortChannelList 		(size_t index, void *ptrToItem, void *callbackData);
static DAQmxDIPortChannel_type* 	copy_DAQmxDIPortChannel_type		(DAQmxDIPortChannel_type* channel);
static ListType						copy_DAQmxDIPortChannelList			(ListType chanList);

	// DO Line channel
static DAQmxDOLineChannel_type* 	init_DAQmxDOLineChannel_type		(void); 
static void 						discard_DAQmxDOLineChannel_type		(DAQmxDOLineChannel_type** a);
int CVICALLBACK 					DisposeDAQmxDOLineChannelList 		(size_t index, void *ptrToItem, void *callbackData);
static DAQmxDOLineChannel_type* 	copy_DAQmxDOLineChannel_type		(DAQmxDOLineChannel_type* channel);
static ListType						copy_DAQmxDOLineChannelList			(ListType chanList);

	// DO Port channel
static DAQmxDOPortChannel_type* 	init_DAQmxDOPortChannel_type		(void); 
static void 						discard_DAQmxDOPortChannel_type		(DAQmxDOPortChannel_type** a);
int CVICALLBACK 					DisposeDAQmxDOPortChannelList 		(size_t index, void *ptrToItem, void *callbackData);
static DAQmxDOPortChannel_type* 	copy_DAQmxDOPortChannel_type		(DAQmxDOPortChannel_type* channel);
static ListType						copy_DAQmxDOPortChannelList			(ListType chanList);

	// CI channel
static DAQmxCIChannel_type* 		init_DAQmxCIChannel_type			(void); 
static void 						discard_DAQmxCIChannel_type			(DAQmxCIChannel_type** a);
int CVICALLBACK 					DisposeDAQmxCIChannelList 			(size_t index, void *ptrToItem, void *callbackData);
static DAQmxCIChannel_type* 		copy_DAQmxCIChannel_type			(DAQmxCIChannel_type* channel);
static ListType						copy_DAQmxCIChannelList				(ListType chanList);

	// CO channel
static DAQmxCOChannel_type* 		init_DAQmxCOChannel_type			(void); 
static void 						discard_DAQmxCOChannel_type			(DAQmxCOChannel_type** a);
int CVICALLBACK 					DisposeDAQmxCOChannelList 			(size_t index, void *ptrToItem, void *callbackData);
static DAQmxCOChannel_type* 		copy_DAQmxCOChannel_type			(DAQmxCOChannel_type* channel);
static ListType						copy_DAQmxCOChannelList				(ListType chanList);

	// IO mode/type
static ListType						GetSupportedIOTypes					(char devName[], int IOType);
static ListType						GetPhysChanSupportedIOTypes			(char chanName[], int chanIOAttribute);
static void 						PopulateIOMode 						(DAQmxDev_type* dev, int panHndl, int controlID, int ioVal);
static void 						PopulateIOType 						(DAQmxDev_type* dev, int panHndl, int controlID, int ioVal, int ioMode); 

	// ranges
static IORange_type* 				init_IORange_type					(void);
static void 						discard_IORange_type				(IORange_type** a);
static IORange_type*				copy_IORange_type					(IORange_type* IOrange);
static IORange_type*				GetIORanges							(char devName[], int rangeType);
 
	// IO channel management
static IOchan_type* 				init_IOchan_type					(void);
static void 						discard_IOchan_type					(IOchan_type** a);

static void 						init_IOchanSet_type					(IOchanSet_type* a);
static void 						discard_IOchanSet_type				(IOchanSet_type** a);

static int							Load 								(DAQLabModule_type* mod, int workspacePanHndl);

static char* 						substr								(const char* token, char** idxstart);

static BOOL							ValidTaskControllerName				(char name[], void* dataPtr);

static int 							DisplayPanels						(DAQLabModule_type* mod, BOOL visibleFlag);

static int CVICALLBACK 				MainPan_CB							(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static void CVICALLBACK 			ManageDevPan_CB 					(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);
static void CVICALLBACK 			DeleteDev_CB 						(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle); 
static int 							DAQmxDevTaskSet_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


//-----------------------------------------
// NIDAQmx Device Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*			ConfigureTC						(TaskControl_type* taskControl, BOOL const* abortFlag);
static void							IterateTC						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*			StartTC							(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*			DoneTC							(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*			StoppedTC						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 			ResetTC 						(TaskControl_type* taskControl, BOOL const* abortFlag); 
static void				 			ErrorTC 						(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);
static FCallReturn_type*			DataReceivedTC					(TaskControl_type* taskControl, TaskStates_type taskState, CmtTSQHandle dataQID, BOOL const* abortFlag);
static FCallReturn_type*			ModuleEventHandler				(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);  




//========================================================================================================================================================================================================
// Global variables

ListType					devList			= 0;		// List of DAQ devices available of DAQmxDevAttr_type. This will be updated every time init_DevList(ListType) is executed.

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
	nidaq->DAQmxDevices				= ListCreate(sizeof(DAQmxDev_type*));
	if (!nidaq->DAQmxDevices) {discard_DAQLabModule((DAQLabModule_type**)&nidaq); return NULL;}
	
	nidaq->mainPanHndl				= 0;
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
	DAQmxDev_type**			DAQDevPtrPtr;
	
	if (!nidaq) return;
	
	//------------------------------------------
	// discard NIDAQmxManager_type specific data
	//------------------------------------------
	
	// discard DAQmx device data
	for (size_t i = nDAQDevs; i ; i--) {
		DAQDevPtrPtr = ListGetPtrToItem(nidaq->DAQmxDevices, i);
		discard_DAQmxDev_type(DAQDevPtrPtr);
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
// DAQmxDevAttr_type
//------------------------------------------------------------------------------

static DAQmxDevAttr_type* init_DAQmxDevAttr_type(void)
{
	int error = 0;
	
	DAQmxDevAttr_type* a = malloc (sizeof(DAQmxDevAttr_type));
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
	nullChk(a -> AIchan   		= ListCreate (sizeof(DAQmxAIChannel_type*)));
	nullChk(a -> AOchan	  		= ListCreate (sizeof(DAQmxAOChannel_type*)));
	nullChk(a -> DIlines  		= ListCreate (sizeof(DAQmxDILineChannel_type*)));
	nullChk(a -> DIports  		= ListCreate (sizeof(DAQmxDIPortChannel_type*)));
	nullChk(a -> DOlines  		= ListCreate (sizeof(DAQmxDOLineChannel_type*)));
	nullChk(a -> DOports  		= ListCreate (sizeof(DAQmxDOPortChannel_type*)));
	nullChk(a -> CIchan   		= ListCreate (sizeof(DAQmxCIChannel_type*)));
	nullChk(a -> COchan   		= ListCreate (sizeof(DAQmxCOChannel_type*)));
	
	// triggering supported
	a -> AnalogTriggering		= 0;
	a -> DigitalTriggering		= 0;
	
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

static void discard_DAQmxDevAttr_type(DAQmxDevAttr_type** a)
{
	if (!(*a)) return;
	
	OKfree((*a)->name);
	OKfree((*a)->type);
	
	// discard channel lists and free pointers within lists.
	if ((*a)->AIchan) 	ListApplyToEachEx ((*a)->AIchan, 1, DisposeDAQmxAIChannelList, NULL); ListDispose((*a)->AIchan); 
	if ((*a)->AOchan) 	ListApplyToEachEx ((*a)->AOchan, 1, DisposeDAQmxAOChannelList, NULL); ListDispose((*a)->AOchan);	  
	if ((*a)->DIlines) 	ListApplyToEachEx ((*a)->DIlines, 1, DisposeDAQmxDILineChannelList, NULL); ListDispose((*a)->DIlines);  
	if ((*a)->DIports)	ListApplyToEachEx ((*a)->DIports, 1, DisposeDAQmxDIPortChannelList, NULL); ListDispose((*a)->DIports);  
	if ((*a)->DOlines)	ListApplyToEachEx ((*a)->DOlines, 1, DisposeDAQmxDOLineChannelList, NULL); ListDispose((*a)->DOlines);  
	if ((*a)->DOports)	ListApplyToEachEx ((*a)->DOports, 1, DisposeDAQmxDOPortChannelList, NULL); ListDispose((*a)->DOports);  
	if ((*a)->CIchan)	ListApplyToEachEx ((*a)->CIchan, 1, DisposeDAQmxCIChannelList, NULL); ListDispose((*a)->CIchan);   
	if ((*a)->COchan)	ListApplyToEachEx ((*a)->COchan, 1, DisposeDAQmxCOChannelList, NULL); ListDispose((*a)->COchan);
	
	// discard supported IO type lists
	if ((*a)->AISupportedMeasTypes)		ListDispose((*a)->AISupportedMeasTypes);
	if ((*a)->AOSupportedOutputTypes)	ListDispose((*a)->AOSupportedOutputTypes);
	if ((*a)->CISupportedMeasTypes)		ListDispose((*a)->CISupportedMeasTypes);
	if ((*a)->COSupportedOutputTypes)	ListDispose((*a)->COSupportedOutputTypes);
	
	OKfree(*a);
}

static DAQmxDevAttr_type* copy_DAQmxDevAttr_type(DAQmxDevAttr_type* attr)
{
	if (!attr) return NULL; 
	DAQmxDevAttr_type* a = init_DAQmxDevAttr_type();
	if (!a) return NULL; 

	// product ID
	
	if (!(	a -> name					= StrDup (attr -> name))) 							goto Error; 
	if (!(	a -> type 					= StrDup (attr -> type))) 							goto Error;  
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
	
	if (!(	a -> AIchan  				= copy_DAQmxAIChannelList(attr -> AIchan)))			goto Error;
	if (!(	a -> AOchan  				= copy_DAQmxAOChannelList(attr -> AOchan)))			goto Error; 
	if (!(	a -> DIlines 				= copy_DAQmxDILineChannelList(attr -> DIlines)))	goto Error; 	   
	if (!(	a -> DIports 				= copy_DAQmxDIPortChannelList(attr -> DIports)))	goto Error;   
	if (!(	a -> DOlines 				= copy_DAQmxDOLineChannelList(attr -> DOlines)))	goto Error; 		  
	if (!(	a -> DOports 				= copy_DAQmxDOPortChannelList(attr -> DOports)))	goto Error; 		   
	if (!(	a -> CIchan  				= copy_DAQmxCIChannelList(attr -> CIchan)))			goto Error; 		   
	if (!(	a -> COchan  				= copy_DAQmxCOChannelList(attr -> COchan)))			goto Error; 
	
	// trigger support
	
			a -> AnalogTriggering   	= attr -> AnalogTriggering;
			a -> DigitalTriggering		= attr -> DigitalTriggering;
			
	// supported IO types
	
	if (!(	a -> AISupportedMeasTypes	= ListCopy(attr->AISupportedMeasTypes)))			goto Error; 
	if (!(	a -> AOSupportedOutputTypes	= ListCopy(attr->AOSupportedOutputTypes)))			goto Error; 
	if (!(	a -> CISupportedMeasTypes	= ListCopy(attr->CISupportedMeasTypes)))			goto Error; 
	if (!(	a -> COSupportedOutputTypes	= ListCopy(attr->COSupportedOutputTypes)))			goto Error;
	
	return a;
	
	Error:
	discard_DAQmxDevAttr_type(&a);
	return NULL;
}

//------------------------------------------------------------------------------
// DAQmxAIChannel_type
//------------------------------------------------------------------------------
static DAQmxAIChannel_type* init_DAQmxAIChannel_type (void)
{
	DAQmxAIChannel_type* a = malloc (sizeof(DAQmxAIChannel_type));
	if (!a) return NULL;
	
	a->physChanName 		= NULL;
	a->supportedMeasTypes   = 0;
	a->Vrngs				= NULL;
	a->Irngs				= NULL;
	a->terminalCfg			= 0;
	
	return a;
}

static void discard_DAQmxAIChannel_type (DAQmxAIChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedMeasTypes) ListDispose((*a)->supportedMeasTypes);
	discard_IORange_type(&(*a)->Vrngs);
	discard_IORange_type(&(*a)->Irngs);
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxAIChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxAIChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxAIChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxAIChannel_type* copy_DAQmxAIChannel_type (DAQmxAIChannel_type* channel)
{
	DAQmxAIChannel_type* a = init_DAQmxAIChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))			goto Error;
	if (!(	a->supportedMeasTypes		= ListCopy(channel->supportedMeasTypes)))	goto Error;
	if (!(	a->Vrngs					= copy_IORange_type(channel->Vrngs)))		goto Error;  
	if (!(	a->Irngs					= copy_IORange_type(channel->Irngs)))		goto Error;  
			a->terminalCfg				= channel->terminalCfg;
	
	return a;
	
	Error:
	discard_DAQmxAIChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxAIChannel_type* elements
/// HIRET If successful, a list with copied DAQmxAIChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxAIChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(DAQmxAIChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	DAQmxAIChannel_type* 	chanCopy;
	DAQmxAIChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxAIChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxAIChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------
// DAQmxAOChannel_type
//------------------------------------------------------------------------------
static DAQmxAOChannel_type* init_DAQmxAOChannel_type (void)
{
	DAQmxAOChannel_type* a = malloc (sizeof(DAQmxAOChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->supportedOutputTypes		= 0;
	a->Vrngs					= NULL;
	a->Irngs					= NULL;
	a->terminalCfg				= 0;
	
	return a;
}

static void discard_DAQmxAOChannel_type (DAQmxAOChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedOutputTypes) ListDispose((*a)->supportedOutputTypes);
	discard_IORange_type(&(*a)->Vrngs);
	discard_IORange_type(&(*a)->Irngs);
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxAOChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxAOChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxAOChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxAOChannel_type* copy_DAQmxAOChannel_type (DAQmxAOChannel_type* channel)
{
	DAQmxAOChannel_type* a = init_DAQmxAOChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))				goto Error;
	if (!(	a->supportedOutputTypes		= ListCopy(channel->supportedOutputTypes)))		goto Error;
	if (!(	a->Vrngs					= copy_IORange_type(channel->Vrngs)))			goto Error;  
	if (!(	a->Irngs					= copy_IORange_type(channel->Irngs)))			goto Error;  
			a->terminalCfg				= channel->terminalCfg;
	
	return a;
	
	Error:
	discard_DAQmxAOChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxAOChannel_type* elements
/// HIRET If successful, a list with copied DAQmxAOChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxAOChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(DAQmxAOChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	DAQmxAOChannel_type* 	chanCopy;
	DAQmxAOChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxAOChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxAOChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------
// DAQmxDILineChannel_type
//------------------------------------------------------------------------------
static DAQmxDILineChannel_type* init_DAQmxDILineChannel_type (void)
{
	DAQmxDILineChannel_type* a = malloc (sizeof(DAQmxDILineChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->changeDetectSupported	= 0;
	a->sampClkSupported			= 0;
	
	return a;
}

static void discard_DAQmxDILineChannel_type (DAQmxDILineChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxDILineChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxDILineChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxDILineChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxDILineChannel_type* copy_DAQmxDILineChannel_type (DAQmxDILineChannel_type* channel)
{
	DAQmxDILineChannel_type* a = init_DAQmxDILineChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->changeDetectSupported 	= channel->changeDetectSupported;
			a->sampClkSupported		 	= channel->sampClkSupported;
	
	return a;
	
	Error:
	discard_DAQmxDILineChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxDILineChannel_type* elements
/// HIRET If successful, a list with copied DAQmxDILineChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxDILineChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DAQmxDILineChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DAQmxDILineChannel_type* 	chanCopy;
	DAQmxDILineChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxDILineChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxDILineChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------ 
// DAQmxDIPortChannel_type
//------------------------------------------------------------------------------ 
static DAQmxDIPortChannel_type* init_DAQmxDIPortChannel_type (void)
{
	DAQmxDIPortChannel_type* a = malloc (sizeof(DAQmxDIPortChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->changeDetectSupported	= 0;
	a->sampClkSupported			= 0;
	a->portWidth				= 0;
	
	return a;
}

static void discard_DAQmxDIPortChannel_type (DAQmxDIPortChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxDIPortChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxDIPortChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxDIPortChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxDIPortChannel_type* copy_DAQmxDIPortChannel_type (DAQmxDIPortChannel_type* channel)
{
	DAQmxDIPortChannel_type* a = init_DAQmxDIPortChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->changeDetectSupported 	= channel->changeDetectSupported;
			a->sampClkSupported		 	= channel->sampClkSupported;
			a->portWidth				= channel->portWidth;
	
	return a;
	
	Error:
	discard_DAQmxDIPortChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxDIPortChannel_type* elements
/// HIRET If successful, a list with copied DAQmxDIPortChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxDIPortChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DAQmxDIPortChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DAQmxDIPortChannel_type* 	chanCopy;
	DAQmxDIPortChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxDIPortChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxDIPortChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------
// DAQmxDOLineChannel_type
//------------------------------------------------------------------------------
static DAQmxDOLineChannel_type* init_DAQmxDOLineChannel_type (void)
{
	DAQmxDOLineChannel_type* a = malloc (sizeof(DAQmxDOLineChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->sampClkSupported			= 0;
	
	return a;
}

static void discard_DAQmxDOLineChannel_type (DAQmxDOLineChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxDOLineChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxDOLineChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxDOLineChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxDOLineChannel_type* copy_DAQmxDOLineChannel_type (DAQmxDOLineChannel_type* channel)
{
	DAQmxDOLineChannel_type* a = init_DAQmxDOLineChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->sampClkSupported		 	= channel->sampClkSupported;
	
	return a;
	
	Error:
	discard_DAQmxDOLineChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxDOLineChannel_type* elements
/// HIRET If successful, a list with copied DAQmxDOLineChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxDOLineChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DAQmxDOLineChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DAQmxDOLineChannel_type* 	chanCopy;
	DAQmxDOLineChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxDOLineChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxDOLineChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------
// DAQmxDOPortChannel_type
//------------------------------------------------------------------------------
static DAQmxDOPortChannel_type* init_DAQmxDOPortChannel_type (void)
{
	DAQmxDOPortChannel_type* a = malloc (sizeof(DAQmxDOPortChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->sampModes				= 0;
	a->sampClkSupported			= 0;
	a->portWidth				= 0;
	
	return a;
}

static void discard_DAQmxDOPortChannel_type (DAQmxDOPortChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->sampModes) ListDispose((*a)->sampModes);
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxDOPortChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxDOPortChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxDOPortChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxDOPortChannel_type* copy_DAQmxDOPortChannel_type (DAQmxDOPortChannel_type* channel)
{
	DAQmxDOPortChannel_type* a = init_DAQmxDOPortChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))		goto Error;
	if (!(	a->sampModes				= ListCopy(channel->sampModes)))		goto Error;
			a->sampClkSupported		 	= channel->sampClkSupported;
			a->portWidth				= channel->portWidth;
	
	return a;
	
	Error:
	discard_DAQmxDOPortChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxDOPortChannel_type* elements
/// HIRET If successful, a list with copied DAQmxDOPortChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxDOPortChannelList (ListType chanList)
{
	ListType 					dest 		= ListCreate(sizeof(DAQmxDOPortChannel_type*));
	size_t 						n 			= ListNumItems(chanList);
  	DAQmxDOPortChannel_type* 	chanCopy;
	DAQmxDOPortChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxDOPortChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxDOPortChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------
// DAQmxCIChannel_type
//------------------------------------------------------------------------------
static DAQmxCIChannel_type* init_DAQmxCIChannel_type (void)
{
	DAQmxCIChannel_type* a = malloc (sizeof(DAQmxCIChannel_type));
	if (!a) return NULL;
	
	a->physChanName 		= NULL;
	a->supportedMeasTypes   = 0;
	
	return a;
}

static void discard_DAQmxCIChannel_type (DAQmxCIChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedMeasTypes) ListDispose((*a)->supportedMeasTypes);
	
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxCIChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxCIChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxCIChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxCIChannel_type* copy_DAQmxCIChannel_type (DAQmxCIChannel_type* channel)
{
	DAQmxCIChannel_type* a = init_DAQmxCIChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))			goto Error;
	if (!(	a->supportedMeasTypes		= ListCopy(channel->supportedMeasTypes)))	goto Error;
	
	return a;
	
	Error:
	discard_DAQmxCIChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxCIChannel_type* elements
/// HIRET If successful, a list with copied DAQmxCIChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxCIChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(DAQmxCIChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	DAQmxCIChannel_type* 	chanCopy;
	DAQmxCIChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxCIChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxCIChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------
// DAQmxCOChannel_type
//------------------------------------------------------------------------------
static DAQmxCOChannel_type* init_DAQmxCOChannel_type (void)
{
	DAQmxCOChannel_type* a = malloc (sizeof(DAQmxCOChannel_type));
	if (!a) return NULL;
	
	a->physChanName 			= NULL;
	a->supportedOutputTypes   	= 0;
	
	return a;
}

static void discard_DAQmxCOChannel_type (DAQmxCOChannel_type** a)
{
	if (!*a) return;
	
	OKfree((*a)->physChanName);
	if ((*a)->supportedOutputTypes) ListDispose((*a)->supportedOutputTypes);
	
	OKfree(*a);
}

int CVICALLBACK DisposeDAQmxCOChannelList (size_t index, void *ptrToItem, void *callbackData)
{
	DAQmxCOChannel_type** chanPtrPtr = ptrToItem;
	
	discard_DAQmxCOChannel_type(chanPtrPtr);
	return 0;
}

static DAQmxCOChannel_type* copy_DAQmxCOChannel_type (DAQmxCOChannel_type* channel)
{
	DAQmxCOChannel_type* a = init_DAQmxCOChannel_type();
	if (!a) return NULL;
	
	if (!(	a->physChanName 			= StrDup(channel->physChanName)))			goto Error;
	if (!(	a->supportedOutputTypes		= ListCopy(channel->supportedOutputTypes)))	goto Error;
	
	return a;
	
	Error:
	discard_DAQmxCOChannel_type(&a);
	return NULL;
}

/// HIFN Copies a list of DAQmxCOChannel_type* elements
/// HIRET If successful, a list with copied DAQmxCOChannel_type* elements is returned, otherwise 0.
static ListType	copy_DAQmxCOChannelList (ListType chanList)
{
	ListType 				dest 		= ListCreate(sizeof(DAQmxCOChannel_type*));
	size_t 					n 			= ListNumItems(chanList);
  	DAQmxCOChannel_type* 	chanCopy;
	DAQmxCOChannel_type** 	chanPtrPtr 	= NULL;
	
	if (!dest) return 0;
	
	while (n){	  
		chanPtrPtr 	= ListGetPtrToItem(chanList, n);	  
		chanCopy 	= copy_DAQmxCOChannel_type(*chanPtrPtr);
		if (!chanCopy) goto Error;
  		ListInsertItem (dest, &chanCopy, FRONT_OF_LIST);
  		n--;
	}
	
	return dest;
	
	Error:
	ListApplyToEachEx (dest, 1, DisposeDAQmxCOChannelList, NULL); ListDispose(dest);  
	return 0;
}

//------------------------------------------------------------------------------ 
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

/// HIFN Obtains a list of supported IO types for a given physical channel
/// HIPAR chanName/ Physical channel name, e.g. dev1/ai0
/// HIPAR chanIOAttribute/ Measurement or output type, pass one of the following constants: DAQmx_PhysicalChan_AI_SupportedMeasTypes, DAQmx_PhysicalChan_AO_SupportedOutputTypes,
/// HIPAR chanIOAttribute/ DAQmx_PhysicalChan_CI_SupportedMeasTypes, DAQmx_PhysicalChan_CO_SupportedOutputTypes
static ListType	GetPhysChanSupportedIOTypes	(char chanName[], int chanIOAttribute)
{
	ListType 	IOTypes 	= ListCreate(sizeof(int));
	int			nelem		= 0;
	int*		io			= NULL;
	
	if (!IOTypes) return 0;
	
	if((nelem = DAQmxGetPhysicalChanAttribute(chanName, chanIOAttribute, NULL)) < 0) goto Error;
	
	io = malloc (nelem * sizeof(int));
	if (!io) goto Error; // also if nelem = 0, i.e. no IO types found
	
	if (DAQmxGetDeviceAttribute(chanName, chanIOAttribute, io, nelem) < 0) goto Error;
	
	for (size_t i = 0; i < nelem; i++)
		ListInsertItem(IOTypes, &io[i], END_OF_LIST);
	
	return IOTypes;
	
	Error:
	OKfree(io);
	ListDispose(IOTypes);
	return 0;
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
// IOchan_type
//------------------------------------------------------------------------------

static IOchan_type* init_IOchan_type (void)
{
	int error = 0;
	
	IOchan_type* a = malloc (sizeof(IOchan_type));
	if (!a) return NULL;
	
	a -> nAI				= 0;   // # AI chan used
	a -> nAO				= 0;   // # AO chan used
	a -> nDILines			= 0;   // # DI lines used
	a -> nDIPorts			= 0;   // # DI ports used
	a -> nDOLines			= 0;   // # DO lines used
	a -> nDOPorts			= 0;   // # DO ports used
	a -> nCI				= 0;   // # Counter inputs used
	a -> nCO				= 0;   // # Counter outputs used
	
	// init lists
	a -> AIchanSet			= 0;
	a -> AOchanSet			= 0;
	a -> DIlinesSet			= 0;
	a -> DIportsSet			= 0;
	a -> DOlinesSet			= 0;
	a -> DOportsSet			= 0;
	a -> CIchanSet			= 0;
	a -> COchanSet			= 0;
	
	nullChk(a -> AIchanSet	= ListCreate (sizeof(IOchanSet_type)));
	nullChk(a -> AOchanSet	= ListCreate (sizeof(IOchanSet_type)));
	nullChk(a -> DIlinesSet	= ListCreate (sizeof(IOchanSet_type)));
	nullChk(a -> DIportsSet	= ListCreate (sizeof(IOchanSet_type)));
	nullChk(a -> DOlinesSet	= ListCreate (sizeof(IOchanSet_type)));
	nullChk(a -> DOportsSet	= ListCreate (sizeof(IOchanSet_type)));
	nullChk(a -> CIchanSet	= ListCreate (sizeof(IOchanSet_type)));
	nullChk(a -> COchanSet	= ListCreate (sizeof(IOchanSet_type)));
	
	return a;
	
	Error:
	
	if (a->AIchanSet) ListDispose(a->AIchanSet); 
	if (a->AOchanSet) ListDispose(a->AOchanSet);
	if (a->DIlinesSet) ListDispose(a->DIlinesSet); 
	if (a->DIportsSet) ListDispose(a->DIportsSet);
	if (a->DOlinesSet) ListDispose(a->DOlinesSet); 
	if (a->DOportsSet) ListDispose(a->DOportsSet);
	if (a->CIchanSet) ListDispose(a->CIchanSet); 
	if (a->COchanSet) ListDispose(a->COchanSet);
	
	OKfree(a);
	
	return NULL;
}

//------------------------------------------------------------------------------
// IOchanSet_type
//------------------------------------------------------------------------------

static void init_IOchanSet_type(IOchanSet_type* a)
{
  	a->name     = NULL;  // dynamically allocated string
 	a->vname    = NULL;	 // dynamically allocated string
	a->settings = NULL;  // dynamically allocated structure of specific channel settings such as AIAOChanSet_type etc.. 

}

static void discard_IOchanSet_type(IOchanSet_type** a)
{
	if (!(*a)) return;
	
	OKfree((*a) -> name);
	OKfree((*a) -> vname);
	OKfree((*a) -> settings);
	*a = NULL;
}

static void discard_IOchan_type (IOchan_type** a)
{
	if (!(*a)) return;
	
	// free here IOchanSet_type elements from the channel lists
	size_t 				n;
	IOchanSet_type 		iochanset;
	IOchanSet_type* 	iochansetptr;
	#define DISCARDChanSet(list)       				    \
			n = ListNumItems(list); 			        \
			while (n){ 						        	\
				ListGetItem(list, &iochanset, n);	    \
				iochansetptr = &iochanset;			    \
				discard_IOchanSet_type(&iochansetptr);  \
				n--;								    \
			}                                                     
	
	DISCARDChanSet((*a) -> AIchanSet);
	DISCARDChanSet((*a) -> AOchanSet);
	DISCARDChanSet((*a) -> DIlinesSet);
	DISCARDChanSet((*a) -> DIportsSet);
	DISCARDChanSet((*a) -> DOlinesSet);
	DISCARDChanSet((*a) -> DOportsSet);
	DISCARDChanSet((*a) -> CIchanSet);
	DISCARDChanSet((*a) -> COchanSet);
	  
	// dispose lists
	if ((*a) -> AIchanSet) 	{ListDispose((*a) -> AIchanSet); 	(*a) -> AIchanSet 	= 0;}
	if ((*a) -> AOchanSet) 	{ListDispose((*a) -> AOchanSet);	(*a) -> AOchanSet 	= 0;}
	if ((*a) -> DIlinesSet) {ListDispose((*a) -> DIlinesSet);	(*a) -> DIlinesSet 	= 0;}
	if ((*a) -> DIportsSet) {ListDispose((*a) -> DIportsSet); 	(*a) -> DIportsSet	= 0;}
	if ((*a) -> DOlinesSet) {ListDispose((*a) -> DOlinesSet); 	(*a) -> DOlinesSet 	= 0;}
	if ((*a) -> DOportsSet) {ListDispose((*a) -> DOportsSet); 	(*a) -> DOportsSet	= 0;}
	if ((*a) -> CIchanSet) 	{ListDispose((*a) -> CIchanSet); 	(*a) -> CIchanSet	= 0;}
	if ((*a) -> COchanSet) 	{ListDispose((*a) -> COchanSet); 	(*a) -> COchanSet	= 0;} 

	OKfree(*a);
}

//------------------------------------------------------------------------------
// DAQmxDev_type
//------------------------------------------------------------------------------

static DAQmxDev_type* init_DAQmxDev_type (DAQmxDevAttr_type** attr, char taskControllerName[])
{
	DAQmxDev_type* a = malloc (sizeof(DAQmxDev_type));
	if (!a) return NULL;
	
	a -> attr								= *attr;
	a -> chan								= NULL; 
	
	//-------------------------------------------------------------------------------------------------
	// Task Controller
	//-------------------------------------------------------------------------------------------------
	a -> taskController = init_TaskControl_type (taskControllerName, ConfigureTC, IterateTC, StartTC, 
						  ResetTC, DoneTC, StoppedTC, DataReceivedTC, ModuleEventHandler, ErrorTC);
	if (!a->taskController) {discard_DAQmxDevAttr_type(attr); free(a); return NULL;}
	// connect DAQmxDev data to Task Controller
	SetTaskControlModuleData(a -> taskController, a);
	//-------------------------------------------------------------------------------------------------
	

	
	//--------------------------
	// DAQmx task settings
	//--------------------------
	
	for (int i = 0; i < MAX_DAQmx_TASKS; i++) {
		(a -> taskset[i]).measmode              	= MeasNone;
		(a -> taskset[i]).samplerate            	= 50000;	   	// Default [Hz]
		(a -> taskset[i]).nsamples              	= 0;
		(a -> taskset[i]).duration              	= 0; 
		(a -> taskset[i]).blocksize             	= 4096;      	// Must be a power of 2 because of how the DAQmx output buffers work. 
																	// The size of the output buffer is set to twice the blocksize 
		(a -> taskset[i]).starttrig.trigtype    	= TRIG_None;
		(a -> taskset[i]).starttrig.trigsource  	= NULL;      	// Dynamically allocated
		(a -> taskset[i]).starttrig.edgetype    	= 0;
		(a -> taskset[i]).starttrig.level       	= 0;
		(a -> taskset[i]).starttrig.windowtop   	= 0;
		(a -> taskset[i]).starttrig.windowbttm  	= 0;
		(a -> taskset[i]).starttrig.wndtrigcond 	= 0; 
		(a -> taskset[i]).sampclksource 		  	= NULL;      	// Dynamically allocated
		(a -> taskset[i]).sampclkedge		      	= 0;
		(a -> taskset[i]).refclksource 		  		= NULL;      	// Dynamically allocated
		(a -> taskset[i]).refclkfreq		      	= 1e7;       	// In [Hz], here 10 MHz reference clock assumed
		(a -> taskset[i]).timeout			      	= 5;			// Timeout in [s]
	}
	
	return a;
}

static void discard_DAQmxDev_type(DAQmxDev_type** a)
{
	if (!(*a)) return;
	
	discard_DAQmxDevAttr_type(&(*a)->attr);
	
	discard_IOchan_type(&(*a)->chan);
	
	discard_TaskControl_type(&(*a)->taskController);
	
	for (int i = 0; i < MAX_DAQmx_TASKS; i++) {
		OKfree((*a)->taskset[i].starttrig.trigsource);
		OKfree((*a)->taskset[i].sampclksource);
		OKfree((*a)->taskset[i].refclksource);
	}
	
	OKfree(*a);
}

//===============================================================================================================
//									NI DAQ module loading and device management
//===============================================================================================================

int	Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	int						error						= 0;
	NIDAQmxManager_type* 	nidaq						= (NIDAQmxManager_type*) mod;  
	int						menubarHndl					= 0; 
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
	menubarHndl 		= NewMenuBar(nidaq->mainPanHndl);
	menuItemDevicesHndl = NewMenu(menubarHndl, "Devices", -1);
	
	NewMenuItem(menubarHndl, menuItemDevicesHndl, "Add", -1, (VAL_MENUKEY_MODIFIER | 'A'), ManageDevPan_CB, mod); 
	NewMenuItem(menubarHndl, menuItemDevicesHndl, "Delete", -1, (VAL_MENUKEY_MODIFIER | 'D'), DeleteDev_CB, mod);
	
	return 0;
	Error:
	
	if (nidaq->mainPanHndl) {DiscardPanel(nidaq->mainPanHndl); nidaq->mainPanHndl = 0;}
	if (nidaq->devListPanHndl) {DiscardPanel(nidaq->devListPanHndl); nidaq->devListPanHndl = 0;}
	if (nidaq->taskSetPanHndl) {DiscardPanel(nidaq->taskSetPanHndl); nidaq->taskSetPanHndl = 0;} 
	
	return error;
}

/// HIFN Populates table and a devlist of DAQmxDevAttr_type with NIDAQmxManager devices and their properties.
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
	DAQmxDevAttr_type* 			devAttrPtr; 
	DAQmxAIChannel_type*		newAIChanPtr		= NULL;
	DAQmxAOChannel_type*		newAOChanPtr		= NULL;
	DAQmxDILineChannel_type*	newDILineChanPtr	= NULL;
	DAQmxDIPortChannel_type*	newDIPortChanPtr	= NULL;
	DAQmxDOLineChannel_type*	newDOLineChanPtr	= NULL;
	DAQmxDOPortChannel_type*	newDOPortChanPtr	= NULL;
	DAQmxCIChannel_type*		newCIChanPtr		= NULL;
	DAQmxCOChannel_type*		newCOChanPtr		= NULL;
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
		// Init DAQmxDevAttr_type dev structure and fill it with data
		
		if (!(devAttrPtr = init_DAQmxDevAttr_type())) {   
			DLMsg("Error: DAQmxDevAttr_type structure could not be initialized.\n\n", 1);
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
		
		// Macro to fill lists with channel names and count
		#define FILLSTRLIST(property, list, nitems) 									\
				errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, property, NULL));  \
				nullChk(*idxstr = realloc (*idxstr, buffersize)); 						\
				errChk(DAQmxGetDeviceAttribute (dev_pt, property, *idxstr, buffersize));\
				nitems = 0; 															\
				tmpsubstr = substr (", ", idxstr);										\
				while (tmpsubstr != NULL) {												\
					if (!(newChanPtr = init_DAQmxChannel_type())) goto Error;			\
					newChanPtr->physChanName = tmpsubstr;
					newChanPtr->IOTypes = GetPhysChanSupportedIOTypes(tmpsubstr,  
					nitems++; 															\
					ListInsertItem (list, &tmpsubstr, END_OF_LIST);						\
					tmpsubstr = substr (", ", idxstr); 									\
				} 																		\
		//------------------------------------------------									  
		// Channel properties							
		//------------------------------------------------
		
		// 5. AI
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_PhysicalChans, NULL));  
		nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_PhysicalChans, *idxstr, buffersize));
		nAI = 0; 															
		tmpsubstr = substr (", ", idxstr);										
		while (tmpsubstr != NULL) {												
			if (!(newAIChanPtr = init_DAQmxAIChannel_type())) goto Error;			
			newAIChanPtr->physChanName 			= tmpsubstr;
			newAIChanPtr->supportedMeasTypes 	= GetPhysChanSupportedIOTypes(tmpsubstr, DAQmx_PhysicalChan_AI_SupportedMeasTypes);  
			newAIChanPtr->Vrngs					= GetIORanges(dev_pt, DAQmx_Dev_AI_VoltageRngs);
			newAIChanPtr->Irngs					= GetIORanges(dev_pt, DAQmx_Dev_AI_CurrentRngs);
			DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_AI_TermCfgs, &newAIChanPtr->terminalCfg); 
			ListInsertItem (devAttrPtr->AIchan, &newAIChanPtr, END_OF_LIST);						
			tmpsubstr = substr (", ", idxstr); 									
			nAI++; 															
		} 					
		
		// 6. AO
		errChk(buffersize = DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_PhysicalChans, NULL));  
		nullChk(*idxstr = realloc (*idxstr, buffersize)); 						
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_PhysicalChans, *idxstr, buffersize));
		nAO = 0; 															
		tmpsubstr = substr (", ", idxstr);										
		while (tmpsubstr != NULL) {												
			if (!(newAOChanPtr = init_DAQmxAOChannel_type())) goto Error;			
			newAOChanPtr->physChanName 			= tmpsubstr;
			newAOChanPtr->supportedOutputTypes 	= GetPhysChanSupportedIOTypes(tmpsubstr, DAQmx_PhysicalChan_AO_SupportedOutputTypes);  
			newAOChanPtr->Vrngs					= GetIORanges(dev_pt, DAQmx_Dev_AO_VoltageRngs);
			newAOChanPtr->Irngs					= GetIORanges(dev_pt, DAQmx_Dev_AO_CurrentRngs);
			DAQmxGetPhysicalChanAttribute(tmpsubstr, DAQmx_PhysicalChan_AO_TermCfgs, &newAOChanPtr->terminalCfg); 
			ListInsertItem (devAttrPtr->AIchan, &newAIChanPtr, END_OF_LIST);						
			tmpsubstr = substr (", ", idxstr); 									
			nAO++; 															
		} 	
		
		// 7. DI lines
		FILLSTRLIST(DAQmx_Dev_DI_Lines, devAttrPtr->DIlines, nDIlines);
		// 8. DI ports
		FILLSTRLIST(DAQmx_Dev_DI_Ports, devAttrPtr->DIports, nDIports);					
		// 9. DO lines
		FILLSTRLIST(DAQmx_Dev_DO_Lines, devAttrPtr->DOlines, nDOlines);  
		// 10. DO ports
		FILLSTRLIST(DAQmx_Dev_DO_Ports, devAttrPtr->DOports, nDOports);
		// 11. CI 
		FILLSTRLIST(DAQmx_Dev_CI_PhysicalChans, devAttrPtr->CIchan, nCI);  
		// 12. CO 
		FILLSTRLIST(DAQmx_Dev_CO_PhysicalChans, devAttrPtr->COchan, nCO);
		// 13. Single Channel AI max rate
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MaxSingleChanRate, &devAttrPtr->AISCmaxrate)); 	// [Hz]
		// 14. Multiple Channel AI max rate
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MaxMultiChanRate, &devAttrPtr->AIMCmaxrate));  	// [Hz]
		// 15. AI min rate 
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AI_MinRate, &devAttrPtr->AIminrate));  	         	// [Hz] 
		// 16. AO max rate  
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MaxRate, &devAttrPtr->AOmaxrate)); 			 	// [Hz]
		// 17. AO min rate  
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AO_MinRate, &devAttrPtr->AOminrate)); 			 	// [Hz]
		// 18. DI max rate
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DI_MaxRate, &devAttrPtr->DImaxrate)); 			 	// [Hz]
		// 19. DO max rate
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DO_MaxRate, &devAttrPtr->DOmaxrate));    		 		// [Hz]
		
		// --- IO ranges ---
		
		// AI voltage ranges
		devAttrPtr->AIVrngs = GetIORanges(devAttrPtr->name, DAQmx_Dev_AI_VoltageRngs);
		// AO voltage ranges 
		devAttrPtr->AOVrngs = GetIORanges(devAttrPtr->name, DAQmx_Dev_AO_VoltageRngs);
		// AI current ranges
		devAttrPtr->AIIrngs = GetIORanges(devAttrPtr->name, DAQmx_Dev_AI_CurrentRngs);
		// AO current ranges 
		devAttrPtr->AOIrngs = GetIORanges(devAttrPtr->name, DAQmx_Dev_AO_CurrentRngs);
		
		// --- Triggering ---
		
		// Analog triggering supported ?
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_AnlgTrigSupported, &devAttrPtr->AnalogTriggering));
		// Digital triggering supported ?
		errChk(DAQmxGetDeviceAttribute (dev_pt, DAQmx_Dev_DigTrigSupported, &devAttrPtr->DigitalTriggering));
		
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
	discard_DAQmxDevAttr_type(&devAttrPtr);
	return error;
}

static void empty_DevList (ListType devList)
{
    DAQmxDevAttr_type** devPtrPtr;
	
	while (ListNumItems (devList)) {
		devPtrPtr = ListGetPtrToItem (devList, END_OF_LIST);
		discard_DAQmxDevAttr_type (devPtrPtr);
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
		nullChk(devList	= ListCreate(sizeof(DAQmxDevAttr_type*)));
	
	// clear table
	DeleteTableRows(nidaq->devListPanHndl, DevListPan_DAQTable, 1, -1);
	// display device properties table
	DisplayPanel(nidaq->devListPanHndl);
	
	// find devices
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
	DAQmxDev_type**			DAQmxDevPtrPtr;
	int						activeTabIdx;		// 0-based index
	int						nTabPages;
	ListType				TCList; 
	
	// get selected tab index
	GetActiveTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, &activeTabIdx);
	// remove Tab and add an empty "None" tab page if there are no more tabs
	DeleteTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, activeTabIdx, 1);
	GetNumTabPages(nidaq->mainPanHndl, NIDAQmxPan_Devices, &nTabPages);
	if (!nTabPages) InsertTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, 0, "None");
	// get pointer to selected DAQmx device and remove its Task Controller from the framework
	DAQmxDevPtrPtr = ListGetPtrToItem(nidaq->DAQmxDevices, activeTabIdx + 1);
	TCList = ListCreate(sizeof(TaskControl_type*));
	ListInsertItem(TCList, &(*DAQmxDevPtrPtr)->taskController, END_OF_LIST);
	DLRemoveTaskControllers(TCList);
	ListDispose(TCList);
	// remove Task Controller from the list of module Task Controllers
	ListRemoveItem(nidaq->baseClass.taskControllers, 0, activeTabIdx + 1);
	// discard DAQmx device data and remove device also from the module list
	discard_DAQmxDev_type(DAQmxDevPtrPtr);
	ListRemoveItem(nidaq->DAQmxDevices, 0, activeTabIdx + 1);
}

static void PopulateIOMode (DAQmxDev_type* dev, int panHndl, int controlID, int ioVal)
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

static void PopulateIOType (DAQmxDev_type* dev, int panHndl, int controlID, int ioVal, int ioMode)
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
	DAQmxDev_type* 	dev	 		= (DAQmxDev_type*) callbackData;
	int				ioVal;
	int				ioMode;
	
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case TaskSetPan_IO:
					
					GetCtrlVal(panel, TaskSetPan_IO, &ioVal);
					PopulateIOMode(dev, panel, TaskSetPan_IOMode, ioVal); 
					GetCtrlVal(panel, TaskSetPan_IOMode, &ioMode);
					PopulateIOType(dev, panel, TaskSetPan_IOType, ioVal, ioMode);
					break;
					
				case TaskSetPan_IOMode:
					
					GetCtrlVal(panel, TaskSetPan_IO, &ioVal);
					GetCtrlVal(panel, TaskSetPan_IOMode, &ioMode);
					PopulateIOType(dev, panel, TaskSetPan_IOType, ioVal, ioMode);
					break;
			}
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
					DAQmxDev_type*			newDAQmxDev;
					DAQmxDevAttr_type*		newDAQmxDevAttrPtr;
					ListType				newTCList;
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
					newDAQmxDevAttrPtr = copy_DAQmxDevAttr_type(*(DAQmxDevAttr_type**)ListGetPtrToItem(devList, currDev));
					// add new DAQmx device to module list and framework
					newDAQmxDev = init_DAQmxDev_type(&newDAQmxDevAttrPtr, newTCName);
					ListInsertItem(nidaq->DAQmxDevices, &newDAQmxDev, END_OF_LIST);
					ListInsertItem(nidaq->baseClass.taskControllers, &newDAQmxDev->taskController, END_OF_LIST);
					newTCList = ListCreate(sizeof(TaskControl_type*));
					ListInsertItem(newTCList, &newDAQmxDev->taskController, END_OF_LIST);
					DLAddTaskControllers(newTCList);
					ListDispose(newTCList);
					
					// copy DAQmx Task settings panel to module tab and get handle to the panel inserted in the tab
					newTabPageIdx = InsertPanelAsTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, -1, nidaq->taskSetPanHndl);
					GetPanelHandleFromTabPage(nidaq->mainPanHndl, NIDAQmxPan_Devices, newTabPageIdx, &newDAQmxDevPanHndl);
					// change tab title to new Task Controller name
					SetTabPageAttribute(nidaq->mainPanHndl, NIDAQmxPan_Devices, newTabPageIdx, ATTR_LABEL_TEXT, newTCName);
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

//-----------------------------------------
// NIDAQmx Device Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static void	IterateTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
}

static FCallReturn_type* StartTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* DoneTC	(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* StoppedTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}
static FCallReturn_type* ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static void	ErrorTC (TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	

}

static FCallReturn_type* DataReceivedTC	(TaskControl_type* taskControl, TaskStates_type taskState, CmtTSQHandle dataQID, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	DAQmxDev_type*	daqDev	= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}





