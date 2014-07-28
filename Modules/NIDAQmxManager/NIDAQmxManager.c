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
#define	AITSK	0
#define	AOTSK	1  
#define	DITSK	2
#define	DOTSK	3  
#define	CITSK	4
#define	COTSK	5  

//========================================================================================================================================================================================================
// Types

// Ranges for DAQmx IO, stored as pairs of low value and high value, e.g. -10 V, 10 V 
typedef struct {
	int 				Nrngs;   					// number of ranges
	double* 			low;  						// low value 
	double* 			high; 						// high value
} IORange_type;
	
// DAQ device parameters
typedef struct {
	char*        		name;    	   				// dynamically allocated string
	char*        		type;    	   				// dynamically allocated string
	unsigned int		productNum;					// unique product number
	unsigned int		serial;       				// device serial number
	unsigned int   		nAI;          				// # AI chan
	unsigned int   		nAO;          				// # AO chan
	unsigned int   		nDIlines;     				// # DI lines
	unsigned int   		nDIports;     				// # DI ports
	unsigned int   		nDOlines;     				// # DO lines
	unsigned int   		nDOports;     				// # DO ports
	unsigned int   		nCI;          				// # Counter inputs
	unsigned int   		nCO;          				// # Counter outputs
	// The array of char* must be terminated with a NULL, so use malloc for n strings + 1 pointers.
	// If the char* array is not terminated by NULL deallocation will go wrong and unpredictable behavior will occur
	ListType     		AIchan;       				// list of strings
	ListType     		AOchan;	   					// list of strings
	ListType     		DIlines;      				// list of strings
	ListType     		DIports;      				// list of strings
	ListType     		DOlines;      				// list of strings
	ListType     		DOports;      				// list of strings
	ListType     		CIchan;	   					// list of strings
	ListType     		COchan;	   					// list of strings
	double       		AISCmaxrate;  				// Single channel AI max rate   [Hz]
	double       		AIMCmaxrate;  				// Multiple channel AI max rate [Hz]
	double       		AIminrate;    				// AI minimum rate              [Hz]
	double       		AOmaxrate;    				// AO maximum rate              [Hz]
	double       		AOminrate;    				// AO minimum rate              [Hz]
	double       		DImaxrate;    				// DI maximum rate              [Hz]
	double       		DOmaxrate;    				// DO maximum rate              [Hz]
	IORange_type 		AIVrngs;      				// ranges for AI voltage input  [V]
	IORange_type 		AOVrngs;      				// ranges for AO voltage output [V]
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

static DAQmxDev_type* 		init_DAQmxDev_type 				(DAQmxDevAttr_type** attr, char taskControllerName[]);
static void 				discard_DAQmxDev_type			(DAQmxDev_type** a);

static int 					init_DevList 					(ListType devlist, int panHndl, int tableCtrl);
static void 				empty_DevList 					(ListType devList); 

	// device attributes management
static DAQmxDevAttr_type* 	init_DAQmxDevAttr_type			(void); 
static void 				discard_DAQmxDevAttr_type		(DAQmxDevAttr_type** a);
static DAQmxDevAttr_type* 	copy_DAQmxDevAttr_type			(DAQmxDevAttr_type* attr);

static void					init_IORange_type				(IORange_type* range);
static void 				discard_IORange_type			(IORange_type* a);
	// IO channel management
static IOchan_type* 		init_IOchan_type				(void);
static void 				discard_IOchan_type				(IOchan_type** a);

static void 				init_IOchanSet_type				(IOchanSet_type* a);
static void 				discard_IOchanSet_type			(IOchanSet_type** a);

static int					Load 							(DAQLabModule_type* mod, int workspacePanHndl);

static char* 				substr							(const char* token, char** idxstart);

static BOOL					ValidTaskControllerName			(char name[], void* dataPtr);

static int 					DisplayPanels					(DAQLabModule_type* mod, BOOL visibleFlag);

static int CVICALLBACK 		MainPan_CB						(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static void CVICALLBACK 	ManageDevPan_CB 				(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle);
static void CVICALLBACK 	DeleteDev_CB 					(int menuBarHandle, int menuItemID, void *callbackData, int panelHandle); 
static int 					DAQmxDevTaskSet_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);


//-----------------------------------------
// NIDAQmx Device Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	ConfigureTC						(TaskControl_type* taskControl, BOOL const* abortFlag);
static void					IterateTC						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	StartTC							(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	DoneTC							(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	StoppedTC						(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	ResetTC 						(TaskControl_type* taskControl, BOOL const* abortFlag); 
static void				 	ErrorTC 						(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);
static FCallReturn_type*	DataReceivedTC					(TaskControl_type* taskControl, TaskStates_type taskState, CmtTSQHandle dataQID, BOOL const* abortFlag);
static FCallReturn_type*	ModuleEventHandler				(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);  




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
	
	a -> name         	= NULL; // dynamically allocated string
	a -> type         	= NULL; // dynamically allocated string
	a -> productNum		= 0;	// unique product number
	a -> serial       	= 0;   	// device serial number
	a -> nAI          	= 0;   	// # AI chan
	a -> nAO          	= 0;   	// # AO chan
	a -> nDIlines     	= 0;   	// # DI lines
	a -> nDIports     	= 0;   	// # DI ports
	a -> nDOlines     	= 0;   	// # DO lines
	a -> nDOports     	= 0;   	// # DO ports
	a -> nCI          	= 0;   	// # Counter inputs
	a -> nCO          	= 0;   	// # Counter outputs
	a -> AISCmaxrate  	= 0;   	// Single channel AI max rate
	a -> AIMCmaxrate  	= 0;   	// Multiple channel AI max rate
	a -> AIminrate    	= 0;
	a -> AOmaxrate    	= 0;
	a -> AOminrate    	= 0;
	a -> DImaxrate    	= 0;
	a -> DOmaxrate		= 0;
	
	// init lists
	a -> AIchan			= 0;
	a -> AOchan			= 0;
	a -> DIlines		= 0;
	a -> DIports		= 0;
	a -> DOlines		= 0;
	a -> DOports		= 0;
	a -> CIchan			= 0;
	a -> COchan			= 0;
	
	// list of channel names
	nullChk(a -> AIchan   = ListCreate (sizeof(char *)));
	nullChk(a -> AOchan	  = ListCreate (sizeof(char *)));
	nullChk(a -> DIlines  = ListCreate (sizeof(char *)));
	nullChk(a -> DIports  = ListCreate (sizeof(char *)));
	nullChk(a -> DOlines  = ListCreate (sizeof(char *)));
	nullChk(a -> DOports  = ListCreate (sizeof(char *)));
	nullChk(a -> CIchan   = ListCreate (sizeof(char *)));
	nullChk(a -> COchan   = ListCreate (sizeof(char *)));
	
	init_IORange_type(&a->AIVrngs);
	init_IORange_type(&a->AOVrngs);
	
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
	
	discard_IORange_type(&a->AIVrngs);
	discard_IORange_type(&a->AOVrngs);
	
	free(a);
	
	return NULL;
}

static void discard_DAQmxDevAttr_type(DAQmxDevAttr_type** a)
{
	if (!(*a)) return;
	
	OKfree((*a)->name);
	OKfree((*a)->type);
	
	// Empty lists and free pointers within lists. Pointers must be allocated first by malloc, realloc or calloc.
	ListDisposePtrList ((*a)->AIchan); 
	ListDisposePtrList ((*a)->AOchan);	  
	ListDisposePtrList ((*a)->DIlines);  
	ListDisposePtrList ((*a)->DIports);  
	ListDisposePtrList ((*a)->DOlines);  
	ListDisposePtrList ((*a)->DOports);  
	ListDisposePtrList ((*a)->CIchan);   
	ListDisposePtrList ((*a)->COchan);
	
	discard_IORange_type(&(*a)->AIVrngs);
	discard_IORange_type(&(*a)->AOVrngs);
	
	OKfree(*a);
}

static DAQmxDevAttr_type* copy_DAQmxDevAttr_type(DAQmxDevAttr_type* attr)
{
	DAQmxDevAttr_type* a = malloc (sizeof(DAQmxDevAttr_type));
	if (!a || !attr) return NULL;

	// shallow copy dev data
	memcpy(a, attr, sizeof(DAQmxDevAttr_type)); 

	// Deep copy name and type members
	a -> name = StrDup (attr -> name); // !uses malloc, must dispose using free
	a -> type = StrDup (attr -> type); // !uses malloc, must dispose using free
	// Deep copy physical channel lists 
	a -> AIchan  = StringListCpy(attr -> AIchan);
	a -> AOchan  = StringListCpy(attr -> AOchan);
	a -> DIlines = StringListCpy(attr -> DIlines);	   
	a -> DIports = StringListCpy(attr -> DIports);  
	a -> DOlines = StringListCpy(attr -> DOlines);		  
	a -> DOports = StringListCpy(attr -> DOports);		   
	a -> CIchan  = StringListCpy(attr -> CIchan);		   
	a -> COchan  = StringListCpy(attr -> COchan);
	// copy AI ranges
	a -> AIVrngs.Nrngs = attr -> AIVrngs.Nrngs;
	a -> AIVrngs.low   = malloc(attr -> AIVrngs.Nrngs * sizeof(double));
	memcpy(a -> AIVrngs.low, attr -> AIVrngs.low, attr -> AIVrngs.Nrngs * sizeof(double));  
	a -> AIVrngs.high  = malloc(attr -> AIVrngs.Nrngs * sizeof(double));
	memcpy(a -> AIVrngs.high, attr -> AIVrngs.high, attr -> AIVrngs.Nrngs * sizeof(double));
	// copy AO ranges
	a -> AOVrngs.low = malloc(attr -> AOVrngs.Nrngs * sizeof(double));
	memcpy(a -> AOVrngs.low, attr -> AOVrngs.low, attr -> AOVrngs.Nrngs * sizeof(double));  
	a -> AOVrngs.high = malloc(attr -> AOVrngs.Nrngs * sizeof(double));
	memcpy(a -> AOVrngs.high, attr -> AOVrngs.high, attr -> AOVrngs.Nrngs * sizeof(double));
	
	return a;
}

//------------------------------------------------------------------------------
// IORange_type
//------------------------------------------------------------------------------

static void init_IORange_type(IORange_type* range)
{
	range -> Nrngs 	= 0;
	range -> low 	= NULL;
	range -> high 	= NULL;
}

static void discard_IORange_type(IORange_type* a)
{
	a->Nrngs = 0;
	OKfree(a->low);
	OKfree(a->high);
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
	int					error		= 0;
    int 				buffersize  = 0;
	char* 				tmpnames	= NULL;
	char* 				tmpsubstr	= NULL;
	char* 				dev_pt     	= NULL;
	char** 				idxnames  	= NULL;          // Used to break up the names string
	char** 				idxstr    	= NULL;          // Used to break up the other strings like AI channels
	DAQmxDevAttr_type* 	devAttrPtr; 
	int 				ndev        = 0;
	int 				columns     = 0;
	int 				nelem       = 0;
	double* 			rngs     	= NULL;

	
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
					nitems++; 															\
					ListInsertItem (list, &tmpsubstr, END_OF_LIST);						\
					tmpsubstr = substr (", ", idxstr); 									\
				} 																		\
									
		// 5. AI
		FILLSTRLIST(DAQmx_Dev_AI_PhysicalChans, devAttrPtr->AIchan, devAttrPtr->nAI);  
		// 6. AO
		FILLSTRLIST(DAQmx_Dev_AO_PhysicalChans, devAttrPtr->AOchan, devAttrPtr->nAO);
		// 7. DI lines
		FILLSTRLIST(DAQmx_Dev_DI_Lines, devAttrPtr->DIlines, devAttrPtr->nDIlines);
		// 8. DI ports
		FILLSTRLIST(DAQmx_Dev_DI_Ports, devAttrPtr->DIports, devAttrPtr->nDIports);					
		// 9. DO lines
		FILLSTRLIST(DAQmx_Dev_DO_Lines, devAttrPtr->DOlines, devAttrPtr->nDOlines);  
		// 10. DO ports
		FILLSTRLIST(DAQmx_Dev_DO_Ports, devAttrPtr->DOports, devAttrPtr->nDOports);
		// 11. CI 
		FILLSTRLIST(DAQmx_Dev_CI_PhysicalChans, devAttrPtr->CIchan, devAttrPtr->nCI);  
		// 12. CO 
		FILLSTRLIST(DAQmx_Dev_CO_PhysicalChans, devAttrPtr->COchan, devAttrPtr->nCO);
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
		
		/*---Get IO ranges---*/
		
		// AI voltage ranges
		if((nelem = DAQmxGetDeviceAttribute(devAttrPtr->name, DAQmx_Dev_AI_VoltageRngs, NULL)) > 0){
			devAttrPtr->AIVrngs.Nrngs = (int) nelem/2;
			nullChk(rngs = malloc (nelem * sizeof(double)));
			nullChk(devAttrPtr->AIVrngs.low = malloc((devAttrPtr->AIVrngs.Nrngs) * sizeof(double)));
			nullChk(devAttrPtr->AIVrngs.high = malloc((devAttrPtr->AIVrngs.Nrngs) * sizeof(double)));
			errChk(DAQmxGetDeviceAttribute(devAttrPtr->name, DAQmx_Dev_AI_VoltageRngs, rngs, nelem));
			for (size_t i = 0; i < devAttrPtr->AIVrngs.Nrngs; i++){
				devAttrPtr->AIVrngs.low[i] = rngs[2*i];	
				devAttrPtr->AIVrngs.high[i] = rngs[2*i+1];
			}
			
			OKfree(rngs);
		}
		
		// AO voltage ranges
		if((nelem = DAQmxGetDeviceAttribute(devAttrPtr->name, DAQmx_Dev_AO_VoltageRngs, NULL)) > 0){
			devAttrPtr->AOVrngs.Nrngs = (int) nelem/2;
			nullChk(rngs = malloc(nelem * sizeof(double)));
			nullChk(devAttrPtr->AOVrngs.low = malloc((devAttrPtr->AOVrngs.Nrngs) * sizeof(double)));
			nullChk(devAttrPtr->AOVrngs.high = malloc((devAttrPtr->AOVrngs.Nrngs) * sizeof(double)));
			errChk(DAQmxGetDeviceAttribute(devAttrPtr->name, DAQmx_Dev_AO_VoltageRngs, rngs, nelem));
			for (int i = 0; i < devAttrPtr->AOVrngs.Nrngs; i++){
				devAttrPtr->AOVrngs.low[i] = rngs[2*i];	
				devAttrPtr->AOVrngs.high[i] = rngs[2*i+1];
			}
			
			OKfree(rngs);
		}
			
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
		DAQTable(5, devAttrPtr->nAI);
		// 6. # AO chan
		DAQTable(6, devAttrPtr->nAO);
		// 7. # DI lines
		DAQTable(7, devAttrPtr->nDIlines);
		// 8. # DI ports  
		DAQTable(8, devAttrPtr->nDIports);
		// 9. # DO lines
		DAQTable(9, devAttrPtr->nDOlines);
		// 10. # DO ports  
		DAQTable(10, devAttrPtr->nDOports);
		// 11. # CI 
		DAQTable(11, devAttrPtr->nCI);
		// 12. # CO ports  
		DAQTable(12, devAttrPtr->nCO);
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
	int						activeTabPageHndl;
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

static int DAQmxDevTaskSet_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	switch (event)
	{
		case EVENT_COMMIT:
			
			switch (control) {
					
				case TaskSetPan_MeasType:
					
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
				
					OKfree(newTCName);
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





