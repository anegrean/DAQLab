

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
#include "HW_VUPC.h"


//==============================================================================
// Constants

	// VUPhotonCtr UI resource
#define UI_VUPhotonCtr			"./Modules/VUPhotonCtr/UI_VUPhotonCtr.uir"


#define VChanDataTimeout							1e4					// Timeout in [ms] for Sink VChans to receive data  
	// Default number of samples for finite measurement mode
#define DEFAULT_NSAMPLES		5000
  

	// Default sampling rate in [kHz]
#define DEFAULT_SAMPLING_RATE   1000.0

	// Main panel margin that is kept when inserting channel panels, in [pixels]
#define MAIN_PAN_MARGIN			0

	// Horizontal spacing between panels inserted in the main photon counter panel in [pixels]
#define MAIN_PAN_SPACING		5

	// Maximum visible channels in the main photon counter panel
#define MAX_VISIBLE_CHANNELS	4

#define ITER_TIMEOUT 			10

#define VChan_Default_PulseTrainSinkChan		"VUPC Pulsetrain Sink" 
#define HWTrig_VUPC_BaseName					"PC Start"					// for slave HW triggering 


#ifndef errChk
#define errChk(fCall) if (error = (fCall), error < 0) \
{goto Error;} else
#endif

//==============================================================================


//==============================================================================
// Module implementation

struct VUPhotonCtr {

	// SUPER, must be the first member to inherit from

	DAQLabModule_type 	baseClass;

	// DATA
	
		//-------------------------
		// Task Controller
		//-------------------------
		
	TaskControl_type*	taskControl;

		//-------------------------
		// UI
		//-------------------------

	int					mainPanHndl;

	int					chanPanHndl;

	int					statusPanHndl;

	int					settingsPanHndl;

	int 				counterPanHndl;

	int					taskPanHndl;
	
	int					menubarHndl; 


		//-------------------------
		// Channels
		//-------------------------

		// Array of channels of Channel_type*, the index of the array corresponding to the
		// hardware channel index. If a channel is not in use, its value in the array is NULL.

	Channel_type*		channels[MAX_CHANNELS];
	
	//	virtual channel to receive pulsetrain settings
	SinkVChan_type*		pulseTrainVChan;
	
	HWTrigSlave_type*	HWTrigSlave;				// For establishing a task start HW-trigger dependency, this being a slave.        

		//-------------------------
		// Device IO settings
		//-------------------------

		// Number of samples to acquire if in finite mode. This value is set by the device.
	size_t				nSamples;
		// Acquisition rate set by the device in [Hz]. This value is set by the device.
	double				samplingRate;
		// Points to either device set number of samples or virtual channel set number of samples.
		// By default it points to the device number of samples nSamples. In [Hz]
	size_t*				refNSamples;
		// Points to either device set sampling rate or virtual channel set sampling rate.
		// By default it points to the device sampling rate samplingRate. In [Hz]
	double*				refSamplingRate;



	// METHODS

		// Callback to install on controls from selected panel in UI_VUPhotonCtr.uir
		// Override: Optional, to change UI panel behavior.
		// For hardware specific functionality override other methods such as SetPMT_Mode.
	CtrlCallbackPtr		uiCtrlsCB;

		// Default NULL, functionality not implemented.
		// Override: Required, provides hardware specific operation of VUPhotonCtr.
			
		// Sets the Mode of the Selected PMT
	int					(* SetPMT_Mode) 				(VUPhotonCtr_type* vupc, int PMTnr, PMT_Mode_type mode);
		// Sets the Fan of the Selected PMT ON or OFF
	int					(* SetPMT_Fan) 					(VUPhotonCtr_type* vupc, int PMTnr, BOOL value);
		// Sets the Cooling of the Selected PMT ON or OFF
	int					(* SetPMT_Cooling) 				(VUPhotonCtr_type* vupc, int PMTnr, BOOL value);
		// Sets the Gain and Threshold of the Selected PMT
	int					(* SetPMT_GainThresh) 			(VUPhotonCtr_type* vupc, int PMTnr, double gain, double threshold);
		// Updates status display of Photon Counter from structure data
	int					(* UpdateVUPhotonCtrDisplay) 	(VUPhotonCtr_type* vupc);

	void				(* DimWhenRunning)				(VUPhotonCtr_type* vupc, BOOL dimmed);
		// Sets the Test Mode of the PMT Controller
	int					(* SetTestMode) 				(VUPhotonCtr_type* vupc, BOOL value);
		// Resets the PMT Controller
	int					(* ResetController) 			(VUPhotonCtr_type* vupc);
		 // Resets the Fifo of thePMT Controller
	int					(* ResetFifo) 		        	(VUPhotonCtr_type* vupc);
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static Channel_type* 				init_Channel_type 				(VUPhotonCtr_type* vupcInstance, int panHndl, size_t chanIdx, char VChanName[]);

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

static int CVICALLBACK 				VUPCPhotonCounter_CB 			(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static BOOL 						ValidTaskControllerName			(char name[], void* dataPtr);

	// pulsetrain command VChan connected callback
static void							PulseTrainVChan_Connected 		(VChan_type* self, void* VChanOwner, VChan_type* connectedVChan);
	// pulsetrain command VChan disconnected callback
static void							PulseTrainVChan_Disconnected 	(VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan);

static int 							PulseTrainDataReceivedTC 		(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

static int 							PMT_Set_Mode 					(VUPhotonCtr_type* vupc, int PMTnr, PMT_Mode_type mode);
static int 							PMT_Set_Fan 					(VUPhotonCtr_type* vupc, int PMTnr, BOOL value);
static int 							PMT_Set_Cooling 				(VUPhotonCtr_type* vupc, int PMTnr, BOOL value);
static int 							PMT_Set_GainThresh 				(VUPhotonCtr_type* vupc, int PMTnr, double gain, double threshold);
static int 							PMTController_UpdateDisplay 	(VUPhotonCtr_type* vupc);
//void 								PMTController_DimWhenRunning 	(VUPhotonCtr_type* vupc, BOOL dimmed);
static int 							PMTController_SetTestMode		(VUPhotonCtr_type* vupc, BOOL testmode);
static int 							PMTController_Reset				(VUPhotonCtr_type* vupc);
static int 							PMTController_ResetFifo			(VUPhotonCtr_type* vupc);



//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static int 					ConfigureTC 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int 					UnConfigureTC 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);      
static void					IterateTC				(TaskControl_type* taskControl, BOOL const* abortIterationFlag);
static void 				AbortIterationTC 		(TaskControl_type* taskControl, BOOL const* abortFlag);
static int					StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int					DoneTC					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static int					StoppedTC				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static void					DimUITC					(TaskControl_type* taskControl, BOOL dimmed);
static void					TCActive				(TaskControl_type* taskControl, BOOL UITCActiveFlag);
static int				 	ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 
static void				 	ErrorTC 				(TaskControl_type* taskControl, int errorID, char* errorMsg);
static int					ModuleEventHandler		(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo); 


//==============================================================================
// Global variables


//==============================================================================
// Global functions for module data management

/// HIFN  Allocates memory and initializes the VU Photon Counter module.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed.
DAQLabModule_type*	initalloc_VUPhotonCtr (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	VUPhotonCtr_type* 	vupc;
	TaskControl_type*	tc;
	ListType			newTCList;
	char*				newTCName; 
	
	if (!mod) {
		vupc = malloc (sizeof(VUPhotonCtr_type));
		if (!vupc) return NULL;
	} else
		vupc = (VUPhotonCtr_type*) mod;

	// initialize base class
	initalloc_DAQLabModule(&vupc->baseClass, className, instanceName, workspacePanHndl);
	
	// create VUPhotonCtr Task Controller
	tc = init_TaskControl_type (instanceName, vupc, ConfigureTC, UnConfigureTC, IterateTC, AbortIterationTC, StartTC, 
												 	  ResetTC, DoneTC, StoppedTC, DimUITC, TCActive, ModuleEventHandler, ErrorTC); // module data added to the task controller below
	if (!tc) {discard_DAQLabModule((DAQLabModule_type**)&vupc); return NULL;}
	
	//------------------------------------------------------------

	//---------------------------
	// Parent Level 0: DAQLabModule_type
				
		// METHODS

		// overriding methods
	vupc->baseClass.Discard 		= discard_VUPhotonCtr;
	vupc->baseClass.Load			= Load;
	vupc->baseClass.LoadCfg			= NULL; //LoadCfg;
	vupc->baseClass.DisplayPanels	= DisplayPanels;


	//---------------------------
	// Child Level 1: VUPhotonCtr_type

		// DATA
	SetIteratorType(GetTaskControlCurrentIter(tc),Iterator_Waveform); //this mudules produces waveforms
	
	vupc->taskControl   			= tc;
	vupc->mainPanHndl				= 0;
	vupc->statusPanHndl				= 0;
	vupc->settingsPanHndl			= 0;
	vupc->counterPanHndl			= 0;

	for (int i = 0; i < MAX_CHANNELS; i++)
		vupc->channels[i] = NULL;

	vupc->nSamples					= DEFAULT_NSAMPLES;
	vupc->refNSamples				= &vupc->nSamples;	  		// by default point to device set number of samples
	vupc->samplingRate				= DEFAULT_SAMPLING_RATE;
	vupc->refSamplingRate			= &vupc->samplingRate; 		// by default point to device set sampling rate

	vupc->pulseTrainVChan			= NULL;
	
	
		// METHODS
			// assign default controls callback to UI_VUPhotonCtr.uir panel
	vupc->uiCtrlsCB					= VUPCChannel_CB;



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
	
	//---------------------------------------
	// discard VUPhotonCtr_type specific data
	//---------------------------------------
	
	// clean up hardware
	PMTController_Finalize();
	
	// discard Task Controller
	discard_TaskControl_type(&vupc->taskControl);

	// discard channel resources
	for (int i = 0; i < MAX_CHANNELS; i++)
		discard_Channel_type(&vupc->channels[i]);

	//UI
	// main panel and panels loaded into it (channels and task control)
	if (vupc->menubarHndl)  DiscardMenuBar (vupc->menubarHndl);      
	if (vupc->mainPanHndl) {
		DiscardPanel(vupc->mainPanHndl);
		vupc->mainPanHndl = 0;
		vupc->taskPanHndl = 0;
		vupc->chanPanHndl = 0;
	}

	if (vupc->statusPanHndl) 	DiscardPanel(vupc->statusPanHndl);
	if (vupc->settingsPanHndl) 	DiscardPanel(vupc->settingsPanHndl);
	if (vupc->counterPanHndl)	DiscardPanel(vupc->counterPanHndl);
	
	
	// discard pulsetrain SinkVChan   
	if (vupc->pulseTrainVChan) discard_VChan_type((VChan_type**)&vupc->pulseTrainVChan);
	
	DLUnregisterHWTrigSlave(vupc->HWTrigSlave);
	discard_HWTrigSlave_type(&vupc->HWTrigSlave);  
	    
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}

static Channel_type* init_Channel_type (VUPhotonCtr_type* vupcInstance, int panHndl, size_t chanIdx, char VChanName[])
{
	Channel_type* chan = malloc (sizeof(Channel_type));
	if (!chan) return NULL;

	chan->vupcInstance	= vupcInstance;
	chan->VChan			= init_SourceVChan_type(VChanName, DL_Waveform_UShort, chan, NULL, NULL);
	chan->panHndl   	= panHndl;
	chan->chanIdx		= chanIdx;
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

	if ((*chan)->panHndl) {
		DiscardPanel((*chan)->panHndl);
		(*chan)->panHndl = 0;
	}

	// discard SourceVChan
	discard_VChan_type((VChan_type**)&(*chan)->VChan);

	OKfree(*chan);  // this also removes the channel from the device structure
}

void ErrortoTaskController(VUPhotonCtr_type* vupc,int error,char errstring[])
{
	if (vupc->taskControl!=NULL) AbortTaskControlExecution(vupc->taskControl); 
}

static int InitHardware (VUPhotonCtr_type* vupc)
{
	int error = 0;
	
	
	
	errChk(PMTController_Init());
	  
	
//	SetMeasurementMode(vupc->contmode);
Error:
	if (error!=0) ErrortoTaskController(vupc,error,__func__);   
	
	return error;
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	VUPhotonCtr_type* 	vupc 				= (VUPhotonCtr_type*) mod;
	int					menuItemSettingsHndl;
	int					error 				= 0;
	char				buff[50];
	char*				name;

	// init VU Photon Counting hardware

	if (InitHardware((VUPhotonCtr_type*) mod) < 0) return -1;
	
	// add module's task controller to the framework
	DLAddTaskController(mod, vupc->taskControl);

	// load main panel
	vupc->mainPanHndl 		= LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCMain);
	// load settings panel
	vupc->settingsPanHndl   = LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCSet);
		// load Photoncounter status and control panel
	vupc->counterPanHndl	= LoadPanel(workspacePanHndl, UI_VUPhotonCtr, CounterPan);

	// load Task Controller panel
	vupc->taskPanHndl		= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCTask);
	// load channel panel, the dimensions of which will be used to adjust the size of the main panel
	vupc->chanPanHndl		= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCChan);


	// connect module data and user interface callbackFn to all direct controls in the settings and task panels
	SetCtrlsInPanCBInfo(mod, VUPCSettings_CB, vupc->settingsPanHndl);
	SetCtrlsInPanCBInfo(mod, VUPCTask_CB, vupc->taskPanHndl);
	SetCtrlsInPanCBInfo(mod, VUPCPhotonCounter_CB, vupc->counterPanHndl);



	// connect module data to Settings menubar item
	errChk( vupc->menubarHndl = NewMenuBar(vupc->mainPanHndl) );
	errChk( menuItemSettingsHndl = NewMenu(vupc->menubarHndl, "Settings", -1) );
	SetMenuBarAttribute(vupc->menubarHndl, menuItemSettingsHndl, ATTR_CALLBACK_DATA, mod);
	SetMenuBarAttribute(vupc->menubarHndl, menuItemSettingsHndl, ATTR_CALLBACK_FUNCTION_POINTER, MenuSettings_CB);
	SetMenuBarAttribute(vupc->menubarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);

	// change main panel title to module instance name
	SetPanelAttribute(vupc->mainPanHndl, ATTR_TITLE, mod->instanceName);
	// change settings panel title to module instance name and add " settings" to it
	name = StrDup(" settings");
	if (!name) return -1;
	AddStringPrefix(&name, mod->instanceName, -1);
	SetPanelAttribute(vupc->settingsPanHndl, ATTR_TITLE, name);
	OKfree(name);

	// populate settings panel with available channels
	for (int i = 1; i <= MAX_CHANNELS; i++) {
		sprintf(buff,"Ch. %d", i);
		InsertListItem(vupc->settingsPanHndl, VUPCSet_Channels, -1, buff, i);
	}

	// populate measurement mode ring and select Finite measurement mode
	InsertListItem(vupc->settingsPanHndl, VUPCSet_MeasMode, -1, "Continuous",TASK_CONTINUOUS );  
	InsertListItem(vupc->settingsPanHndl, VUPCSet_MeasMode, -1, "Finite", TASK_FINITE);
	
	SetCtrlIndex(vupc->settingsPanHndl, VUPCSet_MeasMode, TASK_FINITE);

	// update acquisition settings display from structure data
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_NSamples, *vupc->refNSamples);
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_SamplingRate, *vupc->refSamplingRate/1000);					// display in [kHz]
	SetCtrlVal(vupc->settingsPanHndl, VUPCSet_Duration, *vupc->refNSamples/(*vupc->refSamplingRate*1000));	// display in [s]


	// add functionality to set PMT mode
	vupc->SetPMT_Mode = PMT_Set_Mode;
	// add functionality to set the PMT cooling fan state
	vupc->SetPMT_Fan = PMT_Set_Fan;
	// add functionality to set the PMT cooling state
	vupc->SetPMT_Cooling = PMT_Set_Cooling;
	// add functionality to set the PMT Gain and Threshold
	vupc->SetPMT_GainThresh = PMT_Set_GainThresh;
	// Updates status display of Photon Counter
	vupc->UpdateVUPhotonCtrDisplay = PMTController_UpdateDisplay;
	//add functionality to set test mode
	vupc->SetTestMode = PMTController_SetTestMode;
	//add functionality to reset PMT controller
	vupc->ResetController = PMTController_Reset;

	// redraw main panel
	RedrawMainPanel(vupc);

	//-------------------------------------------------------------------------
	// add HW Triggers
	//-------------------------------------------------------------------------
						
	//  Slave HW Triggers
	char*	HWTrigName = GetTaskControlName(vupc->taskControl);
	AppendString(&HWTrigName, ": ", -1);
	AppendString(&HWTrigName, HWTrig_VUPC_BaseName, -1);
	vupc->HWTrigSlave		= init_HWTrigSlave_type(HWTrigName);
	OKfree(HWTrigName);
						
	// register HW Triggers with framework
	DLRegisterHWTrigSlave(vupc->HWTrigSlave);
	
	TaskControlEvent(vupc->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);

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


Error:
	return error;
}

static void	RedrawMainPanel (VUPhotonCtr_type* vupc)
{
	size_t		nChannels		= 0;
	int			menubarHeight	= 0;
	int			chanPanHeight	= 0;
	int			chanPanWidth	= 0;
	int			taskPanWidth	= 0;

	// count the number of channels in use
	for (size_t i = 0; i < MAX_CHANNELS; i++)
		if (vupc->channels[i]) nChannels++;

	// get channel panel attributes from the first loaded channel since they are all the same
	GetPanelAttribute(vupc->chanPanHndl, ATTR_HEIGHT, &chanPanHeight);
	GetPanelAttribute(vupc->chanPanHndl, ATTR_WIDTH, &chanPanWidth);

	// get main panel menubar dimensions
	GetPanelAttribute(vupc->mainPanHndl, ATTR_MENU_HEIGHT, &menubarHeight);
	// get task panel width, since height must be the same as for the channel panel
	GetPanelAttribute(vupc->taskPanHndl, ATTR_WIDTH, &taskPanWidth);

	// adjust size of main panel to match the size of one channel plus task panel and space in between
	SetPanelAttribute(vupc->mainPanHndl, ATTR_HEIGHT, chanPanHeight + 2 * MAIN_PAN_MARGIN + menubarHeight);
	SetPanelAttribute(vupc->mainPanHndl, ATTR_WIDTH, chanPanWidth + taskPanWidth + MAIN_PAN_SPACING + 2 * MAIN_PAN_MARGIN);


	if (!nChannels) {
		HidePanel(vupc->taskPanHndl);
		return;
	}

	// reposition channel panels and display them
	nChannels = 0;
	for (int i = 0; i < MAX_CHANNELS; i++)
		if (vupc->channels[i]){
			nChannels++;
			SetPanelAttribute(vupc->channels[i]->panHndl, ATTR_LEFT, (chanPanWidth + MAIN_PAN_SPACING) * (nChannels-1) + MAIN_PAN_MARGIN);
			SetPanelAttribute(vupc->channels[i]->panHndl, ATTR_TOP, MAIN_PAN_MARGIN + menubarHeight);
			DisplayPanel(vupc->channels[i]->panHndl);

		}




	// reposition Task Controller panel to be the end of all channel panels
	SetPanelAttribute(vupc->taskPanHndl, ATTR_LEFT, (chanPanWidth + MAIN_PAN_SPACING) * nChannels + MAIN_PAN_MARGIN);
	SetPanelAttribute(vupc->taskPanHndl, ATTR_TOP, MAIN_PAN_MARGIN + menubarHeight);
	DisplayPanel(vupc->taskPanHndl);

	// adjust main panel width and insert a horizontal scrollbar if necessary, in which case, adjust also the height
	if (nChannels > MAX_VISIBLE_CHANNELS) {
		// adjust size of main panel to its maximum
		SetPanelAttribute(vupc->mainPanHndl, ATTR_WIDTH, chanPanWidth * MAX_VISIBLE_CHANNELS + taskPanWidth +
						  MAIN_PAN_SPACING * MAX_VISIBLE_CHANNELS + 2 * MAIN_PAN_MARGIN);
		// increase height and add scrollbars
		SetPanelAttribute(vupc->mainPanHndl, ATTR_HEIGHT, chanPanHeight + 2 *  MAIN_PAN_MARGIN + VAL_LARGE_SCROLL_BARS + menubarHeight);
		SetPanelAttribute(vupc->mainPanHndl, ATTR_SCROLL_BARS, VAL_HORIZ_SCROLL_BAR);
	} else {
		// adjust size of main panel to match the size of the required number of channels plus Task Controller panel
		SetPanelAttribute(vupc->mainPanHndl, ATTR_WIDTH, chanPanWidth * nChannels + taskPanWidth + MAIN_PAN_SPACING * nChannels + 2 * MAIN_PAN_MARGIN);
		SetPanelAttribute(vupc->mainPanHndl, ATTR_HEIGHT, chanPanHeight + 2 * MAIN_PAN_MARGIN + menubarHeight);
		// remove scroll bars
		SetPanelAttribute(vupc->mainPanHndl, ATTR_SCROLL_BARS, VAL_NO_SCROLL_BARS);
	}

	//?
	PMTController_UpdateDisplay(vupc);
	DisplayPanel(vupc->counterPanHndl);

}




//resets the VUPC user interface corresponding to the resetted hardware
void ResetVUPC_UI(VUPhotonCtr_type* vupc)
{
	int 	i;
	double 	zero	= 0.0;

	for(i=0;i<MAX_CHANNELS;i++)  {
		if(vupc->channels[i]!=NULL) {
			if(i==0){
			//PMT1 is selected
			 
			}
			if(i==1){
			//PMT2 is selected
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Fan,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Cooling,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Mode,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, 0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Threshold, zero);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Gain, zero);
			}
			if(i==2){
			//PMT3 is selected
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Fan,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Cooling,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Mode,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, 0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Threshold, zero);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Gain, zero);
			}
			if(i==3){
			//PMT4 is selected
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Fan,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Cooling,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Mode,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, 0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Threshold, zero);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Gain, zero);
			}
		}
	}
}



static int PMT_Set_Mode (VUPhotonCtr_type* self, int PMTnr, PMT_Mode_type mode)
{
	int error = 0;

	errChk(PMT_SetMode (PMTnr,mode));

Error:
	return error;
}


static int PMT_Set_Fan (VUPhotonCtr_type* self, int PMTnr, BOOL value)
{
	int error = 0;

	errChk(PMT_SetFan (PMTnr,value));

Error:
	return error;
}

static int PMT_Set_Cooling (VUPhotonCtr_type* self, int PMTnr, BOOL value)
{
	int error = 0;

	errChk(PMT_SetCooling (PMTnr,value));

Error:
	return error;
}

///  HIFN  Sets the PMT gain
///  HIPAR PMTnr/PMT number,PMTGain /Gain, range ......
///  HIRET returns error, no error when 0
int SetPMTGainTresh(int PMTnr,unsigned int PMTGain,unsigned int PMTThreshold)
{
	int error=0;

	errChk(PMT_SetGainTresh(PMTnr,PMTGain,PMTThreshold));

Error:
	return error;
}


int ConvertVoltsToBits(float value_in_volts)
{
	  int 		value_in_bits;
	  double 	voltsperbit	=1/65535.0;
	  
	  //1V corresponds with a bitvalue of 65535
	  value_in_bits=value_in_volts/voltsperbit;
	  
	  return value_in_bits;
}


static int PMT_Set_GainThresh (VUPhotonCtr_type* self, int PMTnr, double gain, double threshold)
{
	int 			error = 0;
	unsigned int 	gain_in_bits;
	unsigned int 	threshold_in_bits;

	gain_in_bits=ConvertVoltsToBits(gain);
	threshold_in_bits=ConvertVoltsToBits(threshold);
	errChk(SetPMTGainTresh(PMTnr,gain_in_bits,threshold_in_bits));

Error:
	return error;
}

int ResetActions(VUPhotonCtr_type* 	vupc)
{
	int error = 0;
	
	errChk(PMTReset());
	SetCtrlVal (vupc->counterPanHndl,CounterPan_BTTN_TestMode, FALSE);
	ResetVUPC_UI(vupc);
	PMTController_UpdateDisplay(vupc);
	
Error:
	return error;
}


static int PMTController_SetTestMode (VUPhotonCtr_type* vupc, BOOL testmode)
{
	int error = 0;

	errChk(PMT_SetTestMode(testmode));
	
Error:
	return error;
}

static int PMTController_Reset (VUPhotonCtr_type* vupc)
{
	int error = 0;

	errChk(ResetActions(vupc));

Error:
	return error;
}


static int PMTController_ResetFifo (VUPhotonCtr_type* vupc)
{
	int error = 0;

	errChk(PMTClearFifo());

Error:
	return error;
}


static int PMTController_UpdateDisplay (VUPhotonCtr_type* vupc)
{
	int 			error 		= 0;
	int 			i;
	BOOL 			HV;
	BOOL 			CurrentErr;
	unsigned long 	statreg;
	unsigned long 	controlreg; 

	//Get status from Hardware
	errChk(ReadPMTReg(CTRL_REG,&controlreg));
	errChk(ReadPMTReg(STAT_REG,&statreg));

	//Update UI
	// photon counter
	SetCtrlVal (vupc->counterPanHndl,CounterPan_NUM_STATUS, statreg);
	SetCtrlVal (vupc->counterPanHndl,CounterPan_NUM_COMMAND, controlreg);
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_RUNNING, statreg&RUNNING_BIT);
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_FIFO_ALMFULL, statreg&FFALMFULL_BIT);
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_FIFO_FULL, statreg&FFOVERFLOW_BIT);
	
	
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_DOOR_OPEN, statreg&DOOROPEN_BIT);
	SetCtrlVal (vupc->counterPanHndl,CounterPan_LED_COUNTEROVERFLOW, statreg&COVERFLOW_BIT);

	for(i=0;i<MAX_CHANNELS;i++)  {
		if(vupc->channels[i]) {
			if(vupc->channels[i]->chanIdx==PMT1){
			//PMT1 is selected
				HV=controlreg&PMT1HV_BIT;
				CurrentErr=statreg&PMT1CURR_BIT;

				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);

				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT1TEMP_BIT);
				
			}
			if(vupc->channels[i]->chanIdx==PMT2){
			//PMT2 is selected
				HV=controlreg&PMT2HV_BIT;
				CurrentErr=statreg&PMT2CURR_BIT;

				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT2TEMP_BIT);
			}
			if(vupc->channels[i]->chanIdx==PMT3){
			//PMT3 is selected
				HV=controlreg&PMT3HV_BIT;
				CurrentErr=statreg&PMT3CURR_BIT;

				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT3TEMP_BIT);
			}
			if(vupc->channels[i]->chanIdx==PMT4){
			//PMT4 is selected
				HV=controlreg&PMT4HV_BIT;
				CurrentErr=statreg&PMT4CURR_BIT;

				if (CurrentErr) SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_RED);
				else SetCtrlAttribute(vupc->channels[i]->panHndl,VUPCChan_LED_STATE1, ATTR_ON_COLOR, VAL_GREEN);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,HV||CurrentErr);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, statreg&PMT4TEMP_BIT);
			}
		}
	}
	
	//clear fifo bits if set
	if ((statreg&FFALMFULL_BIT)||(statreg&FFOVERFLOW_BIT)){ 
			if (statreg&FFALMFULL_BIT) statreg=statreg&~FFALMFULL_BIT;  //clear bit
			if (statreg&FFOVERFLOW_BIT) statreg=statreg&~FFOVERFLOW_BIT;  //clear bit    
			errChk(WritePMTReg(STAT_REG,statreg));    
	}

Error:
	return error;
}




static int CVICALLBACK VUPCChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	Channel_type*	chan 	= callbackData;
	int 			intval;
	float 			gain;
	float 			thresh_mV;
	float 			thresh;
	int 			error	= 0;

	switch (event) {

		case EVENT_COMMIT:

			switch (control) {

				case VUPCChan_Mode:
					GetCtrlVal(panel,control,&intval);
					errChk(PMT_Set_Mode (chan->vupcInstance, chan->chanIdx,intval));
					break;

				case VUPCChan_Gain:
					//get gain value
					GetCtrlVal(panel,control,&gain);
					//get threshold value
					GetCtrlVal(panel,control-1,&thresh_mV);  //relies on threshold control being 1 lower than gain control (Lex; nasty but works)
					thresh=thresh_mV/1000;	   				 //convert to volts
					errChk(PMT_Set_GainThresh (chan->vupcInstance, chan->chanIdx,gain,thresh));
					break;

				case VUPCChan_Threshold:
					//get threshold value
					GetCtrlVal(panel,control,&thresh_mV);
					thresh=thresh_mV/1000; 					//convert to volts
					//get gain value
					GetCtrlVal(panel,control+1,&gain);      //relies on threshold control being 1 lower than gain control (Lex; nasty but works)

					errChk(PMT_Set_GainThresh (chan->vupcInstance, chan->chanIdx,gain,thresh));
					break;

				case VUPCChan_Cooling:
					GetCtrlVal(panel,control,&intval);
					errChk(PMT_Set_Cooling (chan->vupcInstance, chan->chanIdx,intval));
					break;

				case VUPCChan_Fan:
					GetCtrlVal(panel,control,&intval);
					errChk(PMT_Set_Fan (chan->vupcInstance, chan->chanIdx,intval));
					break;
			}
			PMTController_UpdateDisplay(chan->vupcInstance);
			break;
	}

Error:
	return error;
}

static int CVICALLBACK 	VUPCSettings_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc 		=   callbackData;
	char*				vChanName;
	char				buff[DAQLAB_MAX_VCHAN_NAME + 50];
	double 				duration;
	int 				nsamples;
	int 				mode;

	switch (event) {

		case EVENT_COMMIT:

			switch (control) {

				case VUPCSet_MeasMode:
					GetCtrlIndex(panel,VUPCSet_MeasMode,&mode);
					SetTaskControlMode(vupc->taskControl,mode);
					GetCtrlVal(panel,VUPCSet_SamplingRate,vupc->refSamplingRate );  
					GetCtrlVal(panel,VUPCSet_NSamples,&nsamples);   
					*vupc->refNSamples=nsamples;
					Setnrsamples_in_iteration(GetTaskControlMode(vupc->taskControl),*vupc->refSamplingRate,*vupc->refNSamples);
					break;

				case VUPCSet_SamplingRate:
					GetCtrlIndex(panel,VUPCSet_MeasMode,&mode);
					SetTaskControlMode(vupc->taskControl,mode);    
					GetCtrlVal(panel,VUPCSet_SamplingRate,vupc->refSamplingRate);  
					GetCtrlVal(panel,VUPCSet_NSamples,&nsamples);   
					*vupc->refNSamples=nsamples;
					Setnrsamples_in_iteration(GetTaskControlMode(vupc->taskControl),*vupc->refSamplingRate,*vupc->refNSamples);
					
					break;

				case VUPCSet_Duration:
					GetCtrlVal(panel,control,&duration);
					break;

				case VUPCSet_NSamples:
					GetCtrlIndex(panel,VUPCSet_MeasMode,&mode);
					SetTaskControlMode(vupc->taskControl,mode);    
					GetCtrlVal(panel,VUPCSet_SamplingRate,vupc->refSamplingRate);  
					GetCtrlVal(panel,VUPCSet_NSamples,vupc->refNSamples);   
					Setnrsamples_in_iteration(GetTaskControlMode(vupc->taskControl),*vupc->refSamplingRate,*vupc->refNSamples);
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
					// eventData2 =  0-based itemIndex of the item whose mark state is being changed

					SourceVChan_type* 	srcVChan;
					Channel_type*		chan;

					if (eventData1) {
						// channel checked, give new VChan name and add channel
						vChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
						if (!vChanName) {
							// uncheck channel
							CheckListItem(panel, control, eventData2, 0);
							return 0;	// action cancelled
						}
						// rename label in settings panel
						sprintf(buff, "Ch. %d: %s", eventData2+1, vChanName);
						ReplaceListItem(panel, control, eventData2, buff, eventData2+1);
						// create channel
						chan = init_Channel_type(vupc, LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCChan), eventData2+1, vChanName);
						// rename channel mode label
						sprintf(buff, "Ch. %d", eventData2+1);
						SetCtrlAttribute(chan->panHndl, VUPCChan_Mode, ATTR_LABEL_TEXT, buff);
						// register VChan with DAQLab
						DLRegisterVChan((DAQLabModule_type*)chan->vupcInstance, (VChan_type*)chan->VChan);
						
						
						// connect module data and user interface callbackFn to all direct controls in the panel
						SetCtrlsInPanCBInfo(chan, ((VUPhotonCtr_type*)vupc)->uiCtrlsCB, chan->panHndl);
						
						//add sink to receive pulsetrain settings
						if (vupc->pulseTrainVChan==NULL){
							char*	pulsetrainVChanName		= DLGetUniqueVChanName(VChan_Default_PulseTrainSinkChan);
							DLDataTypes allowedPacketTypes[] = {DL_PulseTrain_Freq, DL_PulseTrain_Ticks, DL_PulseTrain_Time};
							vupc->pulseTrainVChan= init_SinkVChan_type(pulsetrainVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), chan->vupcInstance,VChanDataTimeout, PulseTrainVChan_Connected, PulseTrainVChan_Disconnected); 
							// register VChan with DAQLab
							DLRegisterVChan((DAQLabModule_type*)vupc, (VChan_type*)vupc->pulseTrainVChan);	
							AddSinkVChan(vupc->taskControl, vupc->pulseTrainVChan, PulseTrainDataReceivedTC);  
						}
						// update main panel
						RedrawMainPanel(vupc);


					} else {
						// channel unchecked, remove channel

							//clear corresponding control bits in control register
						PMT_ClearControl(vupc->channels[eventData2]->chanIdx);

						// get channel pointer
						srcVChan = vupc->channels[eventData2]->VChan;
						// unregister VChan from DAQLab framework
						DLUnregisterVChan((DAQLabModule_type*)chan->vupcInstance, (VChan_type*)srcVChan);
						// discard channel data
						discard_Channel_type(&vupc->channels[eventData2]);
						// update channel list in the settings panel
						sprintf(buff, "Ch. %d", eventData2+1);
						ReplaceListItem(panel, control, eventData2, buff, eventData2+1);
						// update main panel
						RedrawMainPanel(vupc);

					}

					break;
			}
	}
	return 0;
}

static int CVICALLBACK VUPCTask_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc 	=   callbackData;

	int 				mode;
	int 				repeat;
	double 				waitBetweenIterations;

	switch (event) {
		case EVENT_COMMIT:
			switch (control) {
				case VUPCTask_Mode:		  //repeat mode
					GetCtrlVal(panel,control,&mode);
					SetTaskControlMode	(vupc->taskControl,mode);
					//set Rio module in right mode
					break;
				case VUPCTask_Repeat:
					GetCtrlVal(panel,control,&repeat);
					SetTaskControlIterations (vupc->taskControl, repeat+1);
					break;

				case VUPCTask_Wait:
					GetCtrlVal(panel,control,&waitBetweenIterations);
					SetTaskControlIterationsWait (vupc->taskControl,waitBetweenIterations);
					//prog RIO module
					break;
			}
			// (re) configure Photoncounter Task Controller
			TaskControlEvent(vupc->taskControl, TASK_EVENT_CONFIGURE, NULL, NULL);
			break;
	}
	return 0;
}




static int CVICALLBACK VUPCPhotonCounter_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc 	= callbackData;
	BOOL 				testmode;
	int 				error   = 0;
	int 				start;

	switch (event) {
		case EVENT_COMMIT:
			switch (control) {
				case CounterPan_BTTN_FFRESET:
					errChk(PMTClearFifo());
					PMTController_UpdateDisplay(vupc);
					break;
				case CounterPan_BTTN_RESET :
					errChk(ResetActions(vupc));
					break;

				case CounterPan_BTTN_TestMode:
					GetCtrlVal(panel,control,&testmode);
					errChk(PMT_SetTestMode(testmode));
					PMTController_UpdateDisplay(vupc);
					break;

			}
			break;
	}
	
Error:
	return 0;
}

static BOOL ValidTaskControllerName	(char name[], void* dataPtr)
{
	return DLValidTaskControllerName(name);
}

void HWIterationDone(TaskControl_type* taskControl,int errorID, char errorInfo[])
{
	//hardware informs that the current iteration is done	
	TaskControlIterationDone (taskControl, errorID, errorInfo,TRUE);	//?
}

void HWError(void)
{
	//hardware informs that an error occured during the current iteration	
}


static void VUPC_SetStepCounter	(VUPhotonCtr_type* 	vupc, size_t val)
{
	SetCtrlVal(vupc->taskPanHndl, VUPCTask_TotalIterations, val);
}



void CVICALLBACK MenuSettings_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	VUPhotonCtr_type* 	vupc 	=   callbackData;

	DisplayPanel(vupc->settingsPanHndl);
}

//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	
	return 0;
}


static int UnConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void	IterateTC	(TaskControl_type* taskControl, BOOL const* abortIterationFlag)
{
	VUPhotonCtr_type* 		vupc 				= GetTaskControlModuleData(taskControl);
	double 					timeout				= 3.0;
	double 					delaystep			= 0.1;
	int mode;
	DataPacket_type*		dataPacket			= NULL;
	char*					errMsg				= NULL;
	int						error				= 0;
	void*					dataPacketDataPtr;
	DLDataTypes				dataPacketDataType;
	PulseTrain_type*    	pulsetrain;

	//display current iteration index
	VUPC_SetStepCounter(vupc,GetCurrentIterationIndex(GetTaskControlCurrentIter(taskControl)));
	
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive pulse train settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	if (IsVChanConnected((VChan_type*)vupc->pulseTrainVChan)) {
		errChk( GetDataPacket(vupc->pulseTrainVChan, &dataPacket, &errMsg) );
		dataPacketDataPtr = GetDataPacketPtrToData(dataPacket, &dataPacketDataType);
		pulsetrain=*(PulseTrain_type**)dataPacketDataPtr;
		vupc->nSamples=GetPulseTrainNPulses(pulsetrain);
		*vupc->refNSamples = vupc->nSamples;
		ReleaseDataPacket(&dataPacket);
		dataPacket = NULL;
	}
	Setnrsamples_in_iteration(GetTaskControlMode(vupc->taskControl),vupc->samplingRate,vupc->nSamples); 
	PMTStartAcq(GetTaskControlMode(vupc->taskControl),vupc->taskControl,vupc->channels);
	
	//inform that slave is armed
	errChk(SetHWTrigSlaveArmedStatus(vupc->HWTrigSlave,&errMsg));
	
Error:
	return error;

}





static void AbortIterationTC (TaskControl_type* taskControl,  BOOL const* abortFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
}

static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);

	//reset hardware iteration counter
	//ResetDataCounter();
	//timeout testvalue
	SetTaskControlIterationTimeout(taskControl,ITER_TIMEOUT);

	return 0;
}

static int DoneTC	(TaskControl_type* taskControl,  BOOL const* abortFlag, char** errorInfo)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);

	PMTStopAcq();
	PMTController_UpdateDisplay(vupc);   

	return 0;
}


static void	TCActive (TaskControl_type* taskControl, BOOL UITCActiveFlag)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl); 
}


static int StoppedTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);

	PMTStopAcq();
	PMTController_UpdateDisplay(vupc);   

	return 0;
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);

	return 0;
}

static void	DimUITC	(TaskControl_type* taskControl, BOOL dimmed)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);\
	
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_NUM_COMMAND, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_NUM_STATUS, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_BTTN_FFRESET, ATTR_DIMMED, dimmed);    
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_BTTN_RESET, ATTR_DIMMED, dimmed);    
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_LED_DOOR_OPEN, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_LED_RUNNING, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_BTTN_TestMode, ATTR_DIMMED, dimmed); 
}

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	VUPhotonCtr_type* 	vupc 	= GetTaskControlModuleData(taskControl);
	int 				error	= 0;

	error=ResetActions(vupc);


	PMTStopAcq();    
	// print error message
	DLMsg(errorMsg, 1);

	PMTController_UpdateDisplay(vupc);


}

static int ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorInfo)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);    
	
	return 0;
}



// pulsetrain command VChan connected callback
static void	PulseTrainVChan_Connected (VChan_type* self, void* VChanOwner, VChan_type* connectedVChan)
{
	
}

// pulsetrain command VChan disconnected callback
static void	PulseTrainVChan_Disconnected (VChan_type* self, void* VChanOwner, VChan_type* disconnectedVChan)
{
	
}

static int PulseTrainDataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)
{
	
	VUPhotonCtr_type* 	vupc				= GetTaskControlModuleData(taskControl);
	FCallReturn_type*	fCallReturn			= NULL;
	unsigned int		nSamples;
	int					error				= 0;
	DataPacket_type**	dataPackets			= NULL;
	size_t				nPackets;
	size_t				nElem;
	void*				dataPacketDataPtr;
	DLDataTypes			dataPacketType;  
	PulseTrain_type*    pulsetrain;
	size_t 				i;
	PulseTrainModes		pulsetrainmode;
	double 				hightime,lowtime;
	char*				errMsg				= NULL;
	
	
	// process data only if task controller is not active
	if (taskActive) return 0;

	// get all available data packets 
    //get LAST!!
	errChk( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errMsg) );
			
	for (i = 0; i < nPackets; i++) {
				
		dataPacketDataPtr = GetDataPacketPtrToData(dataPackets[i], &dataPacketType);
		pulsetrain=*(PulseTrain_type**)dataPacketDataPtr;
		vupc->nSamples=GetPulseTrainNPulses(pulsetrain);
		*vupc->refNSamples = vupc->nSamples;  // changed by adrian, originally it was vupc->refNSamples = vupc->nSamples
		pulsetrainmode=GetPulseTrainMode(pulsetrain); 
		switch (pulsetrainmode){						 
			case PulseTrain_Finite:
				SetTaskControlMode(vupc->taskControl,TASK_FINITE);
				break;
			case PulseTrain_Continuous:
				SetTaskControlMode(vupc->taskControl,TASK_CONTINUOUS);
				break;
		}								   
		//only pass frew info  in DL_PulseTrain_Freq  and DL_PulseTrain_Time mode
		switch (dataPacketType) {
			case DL_PulseTrain_Freq:
				vupc->samplingRate=GetPulseTrainFreqTimingFreq((PulseTrainFreqTiming_type*)pulsetrain);
				SetCtrlVal(vupc->settingsPanHndl,VUPCSet_SamplingRate,vupc->samplingRate/1000);  //in khz
				break;
			case DL_PulseTrain_Time:
				hightime=GetPulseTrainTimeTimingHighTime((PulseTrainTimeTiming_type*)pulsetrain);
				lowtime=GetPulseTrainTimeTimingLowTime((PulseTrainTimeTiming_type*)pulsetrain); 
				vupc->samplingRate=1/(hightime+lowtime);
				SetCtrlVal(vupc->settingsPanHndl,VUPCSet_SamplingRate,vupc->samplingRate/1000);     //in khz  
				break;
			case DL_PulseTrain_Ticks:
				break;
		} 	
						
					//update UI
		SetCtrlVal(vupc->settingsPanHndl,VUPCSet_NSamples,vupc->nSamples);
		SetCtrlIndex(vupc->settingsPanHndl,VUPCSet_MeasMode,GetTaskControlMode(vupc->taskControl));     
					
		ReleaseDataPacket(&dataPackets[i]);
	}
		
	OKfree(dataPackets);				
	
Error:	   
				
	return error;
}
 


