

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
#include <formatio.h>
#include <userint.h>
#include "VUPhotonCtr.h" 
#include "UI_VUPhotonCtr.h"
#include "VChannel.h"

//==============================================================================
// Constants

	// VUPhotonCtr UI resource
#define UI_VUPhotonCtr			"./Modules/VUPhotonCtr/UI_VUPhotonCtr.uir"
	// Maximum number of measurement channels	
#define MAX_CHANELS				4

	// Maximum number of channels displayed at once on the main panel. If there are
	// more channels, the a horizontal scroll bar will be added to the panel.
#define MAX_VISIBLE_CHANELS		4

	// Maximum gain applied to a PMT in [V]
#define MAX_GAIN_VOLTAGE		0.9

	// Default number of samples for finite measurement mode
#define DEFAULT_NSAMPLES		10000

	// Default sampling rate in [kHz]
#define DEFAULT_SAMPLING_RATE   1.0

	// Main panel margin that is kept when inserting channel panels, in [pixels]
#define MAIN_PAN_MARGIN			0

	// Horizontal spacing between panels inserted in the main photon counter panel in [pixels]
#define MAIN_PAN_SPACING		5

	// Maximum visible channels in the main photon counter panel
#define MAX_VISIBLE_CHANNELS	4

//==============================================================================
// Types

typedef struct {
	
	VUPhotonCtr_type*	vupcInstance;	// reference to device that owns the channel
	SourceVChan_type*	VChan;			// virtual channel assigned to this physical channel
	int					panHndl;		// panel handle to keep track of controls
	size_t			   	chanIdx;		// 1-based channel index, e.g. PMT 1. idx = 1
	float				gain;			// gain applied to the detector in [V]
	float				maxGain;		// maximum gain voltage allowed in [V]
	float				threshold;		// discriminator threshold in [mV]
	
} Channel_type;

typedef enum {
	
	MEASMODE_FINITE,
	MEASMODE_CONTINUOUS
	
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
	
	int					chanPanHndl;
	
	int					taskPanHndl;
	
		//-------------------------
		// Channels
		//-------------------------
	
		// Array of channels of Channel_type*, the index of the array corresponding to the
		// hardware channel index. If a channel is not in use, its value in the array is NULL.
	
	Channel_type*		channels[MAX_CHANELS];
	
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
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static Channel_type* 				init_Channel_type 				(VUPhotonCtr_type* vupcInstance, int panHndl, size_t chanIdx);

static void							UpdateAcqSetDisplay				(VUPhotonCtr_type* vupc);

static void							discard_Channel_type			(Channel_type** chan);

static int							InitHardware 					(VUPhotonCtr_type* vupc); 

static int							Load 							(DAQLabModule_type* mod, int workspacePanHndl);

static int							DisplayPanels					(DAQLabModule_type* mod, BOOL visibleFlag);

static void							RedrawMainPanel 				(VUPhotonCtr_type* vupc);

	// UI controls callback for operating the photon counter
static int CVICALLBACK 				VUPCChannel_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// UI controls callback for setting up the photon counter
static int CVICALLBACK 				VUPCSettings_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// UI controls callback for changing the Task Controller settings
static int CVICALLBACK 				VUPCTask_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

void CVICALLBACK 					MenuSettings_CB 				(int menuBar, int menuItem, void *callbackData, int panel);




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
	
	for (int i = 0; i < MAX_CHANELS; i++)
		vupc->channels[i] = NULL;
	
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
	
	if (!vupc) return;
	
	// discard VUPhotonCtr_type specific data
	
	if (vupc->mainPanHndl)
		DiscardPanel(vupc->mainPanHndl);
	
	if (vupc->statusPanHndl)
		DiscardPanel(vupc->statusPanHndl);
	
	if (vupc->settingsPanHndl)
		DiscardPanel(vupc->settingsPanHndl);
	
	if (vupc->chanPanHndl)
		DiscardPanel(vupc->chanPanHndl);
	
	if (vupc->taskPanHndl)
		DiscardPanel(vupc->taskPanHndl);
	
	
	for (int i = 0; i < MAX_CHANELS; i++)
		discard_Channel_type(&vupc->channels[i]);
	
	
	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

static Channel_type* init_Channel_type (VUPhotonCtr_type* vupcInstance, int panHndl, size_t chanIdx)
{
	Channel_type* chan = malloc (sizeof(Channel_type));
	if (!chan) return NULL;
	
	chan->vupcInstance	= vupcInstance;
	chan->VChan			= NULL;
	chan->chanIdx		= chanIdx;
	chan->panHndl   	= panHndl;
	chan->gain			= 0;
	chan->maxGain		= 0;
	chan->threshold		= 0;
	
	// register channel with device structure
	vupcInstance->channels[chanIdx - 1] = chan;
	
	return chan;
}

static void	discard_Channel_type (Channel_type** chan)
{
	if (!*chan) return;
	
	if ((*chan)->panHndl)
		DiscardPanel((*chan)->panHndl);
	
	// unregister channel with device structure
	VUPhotonCtr_type* vupc = (*chan)->vupcInstance; 
	vupc->channels[(*chan)->chanIdx - 1] = NULL;
	
	// discard SourceVChan
	discard_VChan_type((VChan_type**)&(*chan)->VChan);
	
	OKfree(*chan);
}

static int InitHardware (VUPhotonCtr_type* vupc)
{
	return 0;	
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	VUPhotonCtr_type* 	vupc 				= (VUPhotonCtr_type*) mod; 
	int					menubarHndl;
	int					menuItemSettingsHndl;
	int					error 				= 0;
	char				buff[50];
	char*				name;
	
	// init VU Photon Counting hardware
	
	if (InitHardware((VUPhotonCtr_type*) mod) < 0) return -1; 
	
	// load main panel
	vupc->mainPanHndl 		= LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCMain);
	// load settings panel
	vupc->settingsPanHndl   = LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCSet);
	// load channel panel
	vupc->chanPanHndl		= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCChan);
	// load Task Controller panel
	vupc->taskPanHndl		= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCTask);
	
	// connect module data and user interface callbackFn to all direct controls in the settings and task panels
	SetCtrlsInPanCBInfo(mod, VUPCSettings_CB, vupc->settingsPanHndl);
	SetCtrlsInPanCBInfo(mod, VUPCTask_CB, vupc->taskPanHndl);
	// connect module data to Settings menubar item
	errChk( menubarHndl = NewMenuBar(vupc->mainPanHndl) );
	errChk( menuItemSettingsHndl = NewMenu(menubarHndl, "Settings", -1) );
	SetMenuBarAttribute(menubarHndl, menuItemSettingsHndl, ATTR_CALLBACK_DATA, mod);
	SetMenuBarAttribute(menubarHndl, menuItemSettingsHndl, ATTR_CALLBACK_FUNCTION_POINTER, MenuSettings_CB);
	SetMenuBarAttribute(menubarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	
	// change main panel title to module instance name
	SetPanelAttribute(vupc->mainPanHndl, ATTR_TITLE, mod->instanceName);
	// change settings panel title to module instance name and add " settings" to it
	name = StrDup(" settings");
	if (!name) return -1;
	AddStringPrefix(&name, mod->instanceName, -1);
	SetPanelAttribute(vupc->settingsPanHndl, ATTR_TITLE, name);
	OKfree(name);
	
	// populate settings panel with available channels
	for (int i = 1; i <= MAX_CHANELS; i++) {
		sprintf(buff,"ch. %d", i);
		InsertListItem(vupc->settingsPanHndl, VUPCSet_Channels, -1, buff, i);  						 
	}
	
	// populate measurement mode ring and select Finite measurement mode
	InsertListItem(vupc->settingsPanHndl, VUPCSet_MeasMode, -1, "Finite", MEASMODE_FINITE);
	InsertListItem(vupc->settingsPanHndl, VUPCSet_MeasMode, -1, "Continuous", MEASMODE_CONTINUOUS);
	SetCtrlIndex(vupc->settingsPanHndl, VUPCSet_MeasMode, 0);
	
	// update acquisition settings display from structure data
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_NSamples, *vupc->nSamples);
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_SamplingRate, *vupc->samplingRate/1000);					// display in [kHz]
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_Duration, *vupc->nSamples/(*vupc->samplingRate*1000));	// display in [s]
	
	return 0;
	
	Error:
	
	return error;

}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
	VUPhotonCtr_type* 	vupc 	= (VUPhotonCtr_type*) mod; 
	int 				error 	= 0;
	
	if (visibleFlag)
		errChk(	DisplayPanel(vupc->mainPanHndl) );
	else
		errChk( HidePanel(vupc->mainPanHndl) );
	
	return 0;
	
	Error:
	
	return error;
}

static void	RedrawMainPanel (VUPhotonCtr_type* vupc)
{
	size_t		nChannels		= 0;
	int			menubarHeight;
	int			chanPanHeight;
	int			chanPanWidth;
	int			taskPanWidth;
	
	// count the number of channels in use
	for (size_t i = 0; i < MAX_CHANELS; i++)
		if (vupc->channels[i]) nChannels++;
	
	// get channel panel and menubar dimensions
	GetPanelAttribute(vupc->mainPanHndl, ATTR_MENU_HEIGHT, &menubarHeight);
	GetPanelAttribute(vupc->chanPanHndl, ATTR_HEIGHT, &chanPanHeight);
	GetPanelAttribute(vupc->chanPanHndl, ATTR_WIDTH, &chanPanWidth);
	
	// adjust size of main panel to match the size of one channel panel
	SetPanelAttribute(vupc->mainPanHndl, ATTR_HEIGHT, chanPanHeight + 2 * MAIN_PAN_MARGIN + menubarHeight);
	SetPanelAttribute(vupc->mainPanHndl, ATTR_WIDTH, chanPanWidth + 2 * MAIN_PAN_MARGIN); 
	
	if (!nChannels) return; // do nothing if there are no channels in use
	
	// get task panel width, since height must be the same as for the channel panel
	GetPanelAttribute(vupc->taskPanHndl, ATTR_HEIGHT, &taskPanWidth);
	
	// reposition channel panels and display them
	nChannels = 0;
	for (int i = 0; i < MAX_CHANELS; i++) 
		if (vupc->channels[i]){
			nChannels++;
			SetPanelAttribute(vupc->channels[i]->panHndl, ATTR_LEFT, (chanPanWidth + MAIN_PAN_SPACING) * (nChannels-1) + MAIN_PAN_MARGIN);  
			DisplayPanel(vupc->channels[i]->panHndl);
		}
	
	// reposition Task Controller panel to be the end of all channel panels
	SetPanelAttribute(vupc->taskPanHndl, ATTR_LEFT, (chanPanWidth + MAIN_PAN_SPACING) * nChannels + MAIN_PAN_MARGIN);  
	DisplayPanel(vupc->taskPanHndl);
	
	// adjust main panel width and insert a horizontal scrollbar if necessary, in which case, adjust also the height
	if (nChannels > MAX_VISIBLE_CHANNELS) {
		// adjust size of main panel to its maximum
		SetPanelAttribute(vupc->mainPanHndl, ATTR_WIDTH, chanPanWidth * MAX_VISIBLE_CHANNELS + taskPanWidth + 
						  MAIN_PAN_SPACING * MAX_VISIBLE_CHANNELS + 2 * MAIN_PAN_MARGIN);
		// increase height and add scrollbars
		SetPanelAttribute(vupc->mainPanHndl, ATTR_HEIGHT, chanPanHeight + 2 *  MAIN_PAN_MARGIN + VAL_LARGE_SCROLL_BARS);
		SetPanelAttribute(vupc->mainPanHndl, ATTR_SCROLL_BARS, VAL_HORIZ_SCROLL_BAR);
	} else {
		// adjust size of main panel to match the size of the required number of channels plus Task Controller panel
		SetPanelAttribute(vupc->mainPanHndl, ATTR_WIDTH, chanPanWidth * nChannels + taskPanWidth + MAIN_PAN_SPACING * nChannels + 2 * MAIN_PAN_MARGIN);
		SetPanelAttribute(vupc->mainPanHndl, ATTR_HEIGHT, chanPanHeight + 2 * MAIN_PAN_MARGIN); 
		// remove scroll bars
		SetPanelAttribute(vupc->mainPanHndl, ATTR_SCROLL_BARS, VAL_NO_SCROLL_BARS);
	}	 
	
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
	VUPhotonCtr_type* 	vupc 		=   callbackData;
	char*				vChanName;
	char				vChanNameBuff[DAQLAB_MAX_VCHAN_NAME + 50];
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case VUPCSet_MeasMode:
					
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
			
		case EVENT_MARK_STATE_CHANGE:
			
			switch (control) {
					
				case VUPCSet_Channels:
					
					// eventData1 =  The new mark state of the list item
					// eventData2 =  itemIndex of the item whose mark state is being changed
					
					SourceVChan_type* 	srcVChan;
					Channel_type*		chan;
					
					if (eventData1) {
						// channel checked, give new VChan name and add channel
						vChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
						if (!vChanName) return 0;	// action cancelled
						
						sprintf(vChanNameBuff, "ch. %d: %s", eventData2+1, vChanName); 
						ReplaceListItem(panel, control, eventData2, vChanNameBuff, eventData2+1);
						
						// create channel
						chan = init_Channel_type(vupc, DuplicatePanel(vupc->mainPanHndl, vupc->chanPanHndl, "", 0, 0), eventData2+1);  
						
						// create source VChan
						srcVChan = init_SourceVChan_type(vChanName, VCHAN_USHORT, chan, NULL, NULL); 
						
						// register VChan with DAQLab
						DLRegisterVChan(srcVChan);
						
						// update main panel
						RedrawMainPanel(vupc);
						
						
					} else {
						// channel unchecked, remove channel
					}

					
					break;
			}
	}
	
	return 0;
	
}

static int CVICALLBACK VUPCTask_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc 	=   callbackData;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case VUPCTask_Mode:
					
					break;
					
				case VUPCTask_Repeat:
					
					break;
					
				case VUPCTask_Wait:
					
					break;
					
			}
			break;
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

static FCallReturn_type* ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "", 0);
}

