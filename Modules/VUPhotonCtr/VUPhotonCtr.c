#include <formatio.h>
#include <userint.h>
#include "UI_VUPhotonCtr.h"

//==============================================================================
//
// Title:		VUPhotonCtr.c
// Purpose:		A short description of the implementation.
//
// Created on:	17-6-2014 at 20:51:00 by Adrian Negrean.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first 
#include "VUPhotonCtr.h"

//==============================================================================
// Constants

	// VUPhotonCtr UI resource
#define UI_VUPhotonCtr			"./Modules/VUPhotonCtr/UI_VUPhotonCtr.uir"
	// maximum number of measurement channels	
#define MAX_CHANELS				4

	// maximum gain applied to a PMT in [V]
#define MAX_GAIN_VOLTAGE		0.9

	// default number of samples for finite measurement mode
#define DEFAULT_NSAMPLES		10000

	// default sampling rate in [kHz]
#define DEFAULT_SAMPLING_RATE   1.0

//==============================================================================
// Types

typedef struct {
	VUPhotonCtr_type*	vupcInstance;	// reference to device that owns the channel
	void*				VChan;			// virtual channel assigned to this physical channel
	int					panHndl;		// panel handle to keep track of controls
	unsigned char   	idx;			// channel index, e.g. PMT 1. idx = 1
	float				gain;			// gain applied to the detector in [V]
	float				maxGain;		// maximum gain voltage allowed in [V]
	float				threshold;		// discriminator threshold in [mV]
} Channel_type;

typedef enum {
	
} Measurement_type;

//==============================================================================
// Module implementation

struct VUPhotonCtr {
	
	// SUPER, must be the first member to inherit from
	
	DAQLabModule_type 	baseClass;
	
	// DATA
	
		//-------------------------
		// UI
		//-------------------------
	int					mainPanHndl; 
	
	int					statusPanHndl;
	
	int					settingsPanHndl;
	
		//-------------------------
		// Channels
		//-------------------------
	
		// Detector list of channels of Channel_type*
	ListType			UIChannels;
	
		//-------------------------
		// Device IO settings
		//-------------------------
		
		// Number of samples to acquire if in finite mode. This value is set by the device.
	size_t				DevNSamples;
		// Acquisition rate set by the device in [Hz]. This value is set by the device.
	double				DevSamplingRate;
		// Points to either device set number of samples or virtual channel set number of samples.
		// By default it points to the device number of samples DevNSamples. In [Hz]
	size_t*				nSamples;
		// Points to either device set sampling rate or virtual channel set sampling rate.
		// By default it points to the device sampling rate DevSamplingRate. In [Hz]
	double*				samplingRate;
	
		
		
	
	
	// METHODS
	
}

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static Channel_type*				init_Channel_type				(VUPhotonCtr_type*	vupcInstance, int panHndl, unsigned char idx);

static void							UpdateIODisplay					(VUPhotonCtr_type* vupc);

static void							discard_Channel_type			(Channel_type** a);

static int							InitHardware 					(VUPhotonCtr_type* vupc); 

static int							Load 							(DAQLabModule_type* mod, int workspacePanHndl);

static int							DisplayPanels					(DAQLabModule_type* mod, BOOL visibleFlag);
	// UI controls callback for operating the photon counter
static int CVICALLBACK 				VUPCChannel_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
	// UI controls callback for setting up the photon counter
static int CVICALLBACK 				VUPCSettings_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);




//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	ConfigureTC				(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	IterateTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag);
static FCallReturn_type*	DoneTC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type*	StoppedTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);
static FCallReturn_type* 	ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag); 
static FCallReturn_type* 	ErrorTC 				(TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag);
static FCallReturn_type*	ModuleEventHandler		(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);  


//==============================================================================
// Global variables

//==============================================================================
// Global functions for module data management

/// HIFN  Allocates memory and initializes the VU Photon Counter module.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed. 
DAQLabModule_type*	initalloc_VUPhotonCtr (DAQLabModule_type* mod, char className[], char instanceName[]) 
{
	VUPhotonCtr_type* vupc;
	
	if (!mod) {
		vupc = malloc (sizeof(VUPhotonCtr_type));
		if (!vupc) return NULL;
	} else
		vupc = (VUPhotonCtr_type*) mod;
		
	// initialize base class
	initalloc_DAQLabModule(&vupc->baseClass, className, instanceName);
	//------------------------------------------------------------
	
	//---------------------------
	// Parent Level 0: DAQLabModule_type 
	
		// DATA
	vupc->baseClass.taskControl		= init_TaskControl_type (MOD_VUPhotonCtr_NAME, ConfigureTC, IterateTC, StartTC, ResetTC,
												 		     DoneTC, StoppedTC, NULL, ModuleEventHandler, ErrorTC);   
		// connect VUPhotonCtr module data to Task Controller
	SetTaskControlModuleData(vupc->baseClass.taskControl, vupc);
		// METHODS
	
		// overriding methods
	vupc->baseClass.Discard 		= discard_VUPhotonCtr;
	vupc->baseClass.Load			= Load;
	vupc->baseClass.LoadCfg			= NULL; //LoadCfg;
	vupc->baseClass.DisplayPanels	= DisplayPanels;
	
	//---------------------------
	// Child Level 1: VUPhotonCtr_type 
	
		// DATA
	vupc->mainPanHndl				= 0;
	vupc->statusPanHndl				= 0;
	vupc->settingsPanHndl			= 0;
	vupc->UIChannels				= ListCreate(sizeof(Channel_type*));
	vupc->DevNSamples				= DEFAULT_NSAMPLES;
	vupc->nSamples					= &vupc->DevNSamples;	  // by default point to device set number of samples
	vupc->DevSamplingRate			= DEFAULT_SAMPLING_RATE;
	vupc->samplingRate				= &vupc->DevSamplingRate; // by default point to device set sampling rate
	
	
		// METHODS
		
	
	
	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) vupc;
	else
		return NULL;
	
}

/// HIFN Discards VUPhotonCtr_type data but does not free the structure memory.
void discard_VUPhotonCtr (DAQLabModule_type** mod)
{
	VUPhotonCtr_type* 		vupc 			= (VUPhotonCtr_type*) (*mod);
	Channel_type**			chanPtrPtr;
	
	if (!vupc) return;
	
	// discard VUPhotonCtr_type specific data
	
	if (vupc->mainPanHndl)
		DiscardPanel(vupc->mainPanHndl);
	
	if (vupc->statusPanHndl)
		DiscardPanel(vupc->statusPanHndl);
	
	if (vupc->settingsPanHndl)
		DiscardPanel(vupc->settingsPanHndl);
	
	if (vupc->UIChannels) {
		for (int i = 1; i <= ListNumItems(vupc->UIChannels); i++) {
			chanPtrPtr = ListGetPtrToItem(vupc->UIChannels, i);
			discard_Channel_type(chanPtrPtr);
		}
		ListDispose(vupc->UIChannels);
	}
	
	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

static Channel_type* init_Channel_type (VUPhotonCtr_type*	vupcInstance, int panHndl, unsigned char idx)
{
	Channel_type* chan = malloc (sizeof(Channel_type));
	if (!chan) return NULL;
	
	chan->vupcInstance	= vupcInstance;
	chan->VChan			= NULL;
	chan->idx			= idx;
	chan->panHndl   	= panHndl;
	chan->gain			= 0;
	chan->maxGain		= 0;
	chan->threshold		= 0;
	
	return chan;
}

static void	discard_Channel_type (Channel_type** a)
{
	if (!*a) return;
	
	if ((*a)->panHndl)
		DiscardPanel((*a)->panHndl);
	
	OKfree(*a);
}

static int InitHardware (VUPhotonCtr_type* vupc)
{
	
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	VUPhotonCtr_type* 	vupc 				= (VUPhotonCtr_type*) mod; 
	int					menubarID;
	int					error = 0;
	char				buff[50];
	
	// init VU Photon Counting hardware
	if (InitHardware((VUPhotonCtr_type*) mod) < 0) return -1; 
	
	// load main panel
	vupc->mainPanHndl 		= LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCMain);
	// load settings panel
	vupc->settingsPanHndl   = LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCSet);
	
	// connect module data and user interface callbackFn to all direct controls in the settings panel
	SetCtrlsInPanCBInfo(mod, VUPCSettings_CB, vupc->settingsPanHndl);
	// connect module data to Settings menubar item
	errChk(menubarID = GetPanelMenuBar(vupc->mainPanHndl));
	SetMenuBarAttribute(menubarID, 0, ATTR_CALLBACK_DATA, mod);
	
	// change panel title to module instance name
	SetPanelAttribute(vupc->mainPanHndl, ATTR_TITLE, mod->instanceName);
	
	// populate settings panel with available channels
	for (int i = 1; i <= MAX_CHANELS; i++) {
		Fmt(buff,"ch. %s<%d", i);
		InsertListItem(vupc->settingsPanHndl, VUPCSet_Channels, -1, buff, i);  
	}
	
	// update IO settings display from structure data
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_NSamples, *vupc->nSamples);
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_SamplingRate, *vupc->samplingRate/1000);					// display in [kHz]
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_Duration, *vupc->nSamples/(*vupc->samplingRate*1000));	// display in [s]
	
	
	
	return 0;
	
	Error:
	
	return error;

}

static int CVICALLBACK VUPCChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Channel_type*	chan 	= callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case VUPCChan_Mode:
					
					break;
					
				case VUPCChan_Gain:
					
					break;
					
				case VUPCChan_Threshold:
					
					break;
					
				case VUPCChan_Cooling:
					
					break;
					
				case VUPCChan_Fan: 
					
					break;
			}
			
			break;
	}
	
	return 0;
}

static int CVICALLBACK 	VUPCSettings_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc 	=   callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case VUPCSet_MaxGain:
					
					break;
					
				case VUPCSet_SamplingRate:
					
					break;
					
				case VUPCSet_Duration:
					
					break;
					
				case VUPCSet_NSamples: 
					
					break;
					
				case VUPCSet_Close:
					
					HidePanel(vupc->settingsPanHndl);
					
					break;
			}
			
			break;
			
		case MARK_STATE_CHANGE:
			
			switch (control) {
					
				case VUPCSet_Channels:
					
					// eventData1 =  The new mark state of the list item
					// eventData2 =  itemIndex of the item whose mark state is being changed
					
					if (eventData1) {
						// channel checked
						
					} else {
						// channel unchecked
					}

					
					break;
			}
	}
	
	return 0;
	
}

void CVICALLBACK MenuSettings_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	VUPhotonCtr_type* 	vupc 	=   callbackData;
	
	DisplayPanel(vupc->settingsPanHndl);
}

//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* IterateTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* StartTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* DoneTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}
static FCallReturn_type* StoppedTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* ErrorTC (TaskControl_type* taskControl, char* errorMsg, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

static FCallReturn_type* EventHandler (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

