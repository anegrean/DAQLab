

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

#define VChan_Default_PulseTrainSinkChan		"pulsetrain" 
#define HWTrig_VUPC_BaseName					"start trigger"					// for slave HW triggering 


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
	int					acqSettingsPanHndl;
	int 				counterPanHndl;
	int					taskPanHndl;
	int					menubarHndl;
	int					acquisitionMenuItemID;
	int					deviceMenuItemID;


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

	size_t				nSamples;					// Number of samples to acquire if in finite mode.
	double				samplingRate;				// Acquisition rate in [Hz].
	
	
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
	int					(* SetPMT_Gain) 				(VUPhotonCtr_type* vupc, int PMTnr, double gain);
		// Sets the Gain and Threshold of the Selected PMT
	int					(* SetPMT_Thresh) 				(VUPhotonCtr_type* vupc, int PMTnr, double threshold);
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

static Channel_type* 			init_Channel_type 				(VUPhotonCtr_type* vupcInstance, int panHndl, size_t chanIdx, char VChanName[]);

static void						UpdateAcqSetDisplay				(VUPhotonCtr_type* vupc);

static void						discard_Channel_type			(Channel_type** chan);

static int						InitHardware 					(VUPhotonCtr_type* vupc);

static int						Load 							(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

static int						DisplayPanels					(DAQLabModule_type* mod, BOOL visibleFlag);

static void						RedrawMainPanel 				(VUPhotonCtr_type* vupc);

	// UI controls callback for operating the photon counter
static int CVICALLBACK 			VUPCChannel_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// UI controls callback for setting up the photon counter
static int CVICALLBACK 			VUPCSettings_CB					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

	// UI controls callback for changing the Task Controller settings
static int CVICALLBACK 			VUPCTask_CB 					(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

void CVICALLBACK 				AcquisitionSettings_CB 			(int menuBar, int menuItem, void *callbackData, int panel);

void CVICALLBACK 				DeviceSettings_CB 				(int menuBar, int menuItem, void *callbackData, int panel);

static int CVICALLBACK 			VUPCPhotonCounter_CB 			(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);

static BOOL 					ValidTaskControllerName			(char name[], void* dataPtr);

static void						SetVUPCSamplingInfo				(VUPhotonCtr_type* vupc, PulseTrain_type* pulseTrain);

static int 						PMT_Set_Mode 					(VUPhotonCtr_type* vupc, int PMTnr, PMT_Mode_type mode);
static int 						PMT_Set_Fan 					(VUPhotonCtr_type* vupc, int PMTnr, BOOL value);
static int 						PMT_Set_Cooling 				(VUPhotonCtr_type* vupc, int PMTnr, BOOL value);
static int 						PMT_Set_Gain					(VUPhotonCtr_type* vupc, int PMTnr, double gain);
static int 						PMT_Set_Thresh 					(VUPhotonCtr_type* vupc, int PMTnr, double threshold);    
static int 						PMTController_UpdateDisplay 	(VUPhotonCtr_type* vupc);
//void 							PMTController_DimWhenRunning 	(VUPhotonCtr_type* vupc, BOOL dimmed);
static int 						PMTController_SetTestMode		(VUPhotonCtr_type* vupc, BOOL testmode);
static int 						PMTController_Reset				(VUPhotonCtr_type* vupc);
static int 						PMTController_ResetFifo			(VUPhotonCtr_type* vupc);



//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static int 						ConfigureTC 					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int 						UnconfigureTC 					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 

static void						IterateTC						(TaskControl_type* taskControl, Iterator_type* iterator,BOOL const* abortIterationFlag);

static int						StartTC							(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg);

static int						DoneTC							(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int						StoppedTC						(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);

static int 						IterationStopTC 						(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg);   

static int						TaskTreeStateChange				(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);

static void						TCActive						(TaskControl_type* taskControl, BOOL UITCActiveFlag);

static int				 		ResetTC 						(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg); 

static void				 		ErrorTC 						(TaskControl_type* taskControl, int errorID, char* errorMsg);

static int						ModuleEventHandler				(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorMsg); 

static int 						PulseTrainDataReceivedTC 		(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);


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
	tc = init_TaskControl_type (instanceName, vupc, DLGetCommonThreadPoolHndl(), ConfigureTC, UnconfigureTC, IterateTC, StartTC, 
												 	  ResetTC, DoneTC, StoppedTC, IterationStopTC, TaskTreeStateChange, TCActive, ModuleEventHandler, ErrorTC); // module data added to the task controller below
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
	SetIteratorType(GetTaskControlIterator(tc),Iterator_Waveform); //this mudules produces waveforms
	
	vupc->taskControl   			= tc;
	vupc->mainPanHndl				= 0;
	vupc->chanPanHndl				= 0;
	vupc->statusPanHndl				= 0;
	vupc->acqSettingsPanHndl		= 0;
	vupc->counterPanHndl			= 0;
	vupc->taskPanHndl				= 0;
	vupc->menubarHndl				= 0;
	vupc->acquisitionMenuItemID		= 0;
	vupc->deviceMenuItemID			= 0;

	for (int i = 0; i < MAX_CHANNELS; i++)
		vupc->channels[i] = NULL;

	vupc->nSamples					= DEFAULT_NSAMPLES;
	vupc->samplingRate				= DEFAULT_SAMPLING_RATE;
	vupc->pulseTrainVChan			= NULL;
	vupc->HWTrigSlave				= NULL;
	
	
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
	
	// discard channel resources
	for (int i = 0; i < MAX_CHANNELS; i++)
		discard_Channel_type(&vupc->channels[i]);

	// UI
	// main panel and panels loaded into it (channels and task control)
	if (vupc->menubarHndl)  { DiscardMenuBar (vupc->menubarHndl); vupc->menubarHndl = 0; }     
	
	if (vupc->mainPanHndl) {
		DiscardPanel(vupc->mainPanHndl);
		vupc->mainPanHndl = 0;
		vupc->taskPanHndl = 0;
		vupc->chanPanHndl = 0;
	}

	OKfreePanHndl(vupc->statusPanHndl);
	OKfreePanHndl(vupc->acqSettingsPanHndl);
	OKfreePanHndl(vupc->counterPanHndl);
	
	// discard pulsetrain SinkVChan   
	if (vupc->pulseTrainVChan) {
		DLUnregisterVChan((DAQLabModule_type*)vupc, ((VChan_type*) vupc->pulseTrainVChan));
		RemoveSinkVChan(vupc->taskControl, vupc->pulseTrainVChan, NULL);
		discard_VChan_type((VChan_type**)&vupc->pulseTrainVChan);
	}
	
	DLUnregisterHWTrigSlave(vupc->HWTrigSlave);
	discard_HWTrigSlave_type(&vupc->HWTrigSlave);  
	
	// discard Task Controller
	discard_TaskControl_type(&vupc->taskControl);
	
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
	chan->VChan			= init_SourceVChan_type(VChanName, DL_Waveform_UShort, chan, NULL);
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
	DLUnregisterVChan((DAQLabModule_type*)(*chan)->vupcInstance, (VChan_type*)(*chan)->VChan);
	// discard SourceVChan
	discard_VChan_type((VChan_type**)&(*chan)->VChan);

	OKfree(*chan);  // this also removes the channel from the device structure
}

void ErrortoTaskController(VUPhotonCtr_type* vupc, int error, char errstring[])
{
	if (vupc->taskControl!=NULL) AbortTaskControlExecution(vupc->taskControl); 
}

static int InitHardware (VUPhotonCtr_type* vupc)
{
INIT_ERR
	
	errChk(PMTController_Init());
	  
//	SetMeasurementMode(vupc->contmode);
Error:
	
	if (errorInfo.error) 
		ErrortoTaskController(vupc, errorInfo.error, __func__);   
	
	return errorInfo.error;
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
INIT_ERR
	
	VUPhotonCtr_type* 	vupc 					= (VUPhotonCtr_type*) mod;
	int					menuItemSettingsHndl	= 0;
	char				buff[50]				= "";
	char*				name					= NULL;

	// init VU Photon Counting hardware

	if (InitHardware((VUPhotonCtr_type*) mod) < 0) return -1;
	
	// add module's task controller to the framework
	DLAddTaskController(mod, vupc->taskControl);

	// load main panel
	errChk( vupc->mainPanHndl 			= LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCMain) );
	// load settings panel
	errChk( vupc->acqSettingsPanHndl 	= LoadPanel(workspacePanHndl, UI_VUPhotonCtr, VUPCSet) );
	// load Photoncounter status and control panel
	errChk( vupc->counterPanHndl		= LoadPanel(workspacePanHndl, UI_VUPhotonCtr, CounterPan) );

	// load Task Controller panel
	errChk( vupc->taskPanHndl			= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCTask) );
	// load channel panel, the dimensions of which will be used to adjust the size of the main panel
	errChk( vupc->chanPanHndl			= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCChan) );

	// connect module data and user interface callbackFn to all direct controls in the settings and task panels
	SetCtrlsInPanCBInfo(mod, VUPCSettings_CB, vupc->acqSettingsPanHndl);
	SetCtrlsInPanCBInfo(mod, VUPCTask_CB, vupc->taskPanHndl);
	SetCtrlsInPanCBInfo(mod, VUPCPhotonCounter_CB, vupc->counterPanHndl);

	// connect module data to Settings menubar item
	errChk( vupc->menubarHndl = NewMenuBar(vupc->mainPanHndl) );
	errChk( menuItemSettingsHndl = NewMenu(vupc->menubarHndl, "Settings", -1) );
	
	/*
	SetMenuBarAttribute(vupc->menubarHndl, menuItemSettingsHndl, ATTR_CALLBACK_DATA, mod);
	SetMenuBarAttribute(vupc->menubarHndl, menuItemSettingsHndl, ATTR_CALLBACK_FUNCTION_POINTER, AcquisitionSettings_CB);
	SetMenuBarAttribute(vupc->menubarHndl, 0, ATTR_SHOW_IMMEDIATE_ACTION_SYMBOL, 0);
	*/
	errChk( vupc->acquisitionMenuItemID = NewMenuItem(vupc->menubarHndl, menuItemSettingsHndl, "Acquisition", -1, (VAL_MENUKEY_MODIFIER | 'A'), AcquisitionSettings_CB, mod) );
	errChk( vupc->deviceMenuItemID = NewMenuItem(vupc->menubarHndl, menuItemSettingsHndl, "Device", -1, (VAL_MENUKEY_MODIFIER | 'D'), DeviceSettings_CB, mod) );

	// change main panel title to module instance name
	SetPanelAttribute(vupc->mainPanHndl, ATTR_TITLE, mod->instanceName);
	// change settings panel title to module instance name and add " settings" to it
	name = StrDup(" settings");
	if (!name) return -1;
	AddStringPrefix(&name, mod->instanceName, -1);
	SetPanelAttribute(vupc->acqSettingsPanHndl, ATTR_TITLE, name);
	OKfree(name);

	// populate settings panel with available channels
	for (int i = 1; i <= MAX_CHANNELS; i++) {
		sprintf(buff,"Ch. %d", i);
		InsertListItem(vupc->acqSettingsPanHndl, VUPCSet_Channels, -1, buff, i);
	}

	// populate measurement mode ring and select Finite measurement mode
	InsertListItem(vupc->acqSettingsPanHndl, VUPCSet_MeasMode, -1, "Continuous",TASK_CONTINUOUS );  
	InsertListItem(vupc->acqSettingsPanHndl, VUPCSet_MeasMode, -1, "Finite", TASK_FINITE);
	
	SetCtrlIndex(vupc->acqSettingsPanHndl, VUPCSet_MeasMode, TASK_FINITE);

	// update acquisition settings display from structure data
	SetCtrlVal(vupc->acqSettingsPanHndl, VUPCSet_NSamples, (unsigned int)vupc->nSamples);
	SetCtrlVal(vupc->acqSettingsPanHndl, VUPCSet_SamplingRate, vupc->samplingRate/1000);				// display in [kHz]
	SetCtrlVal(vupc->acqSettingsPanHndl, VUPCSet_Duration, vupc->nSamples/vupc->samplingRate);		// display in [s]


	// add functionality to set PMT mode
	vupc->SetPMT_Mode = PMT_Set_Mode;
	// add functionality to set the PMT cooling fan state
	vupc->SetPMT_Fan = PMT_Set_Fan;
	// add functionality to set the PMT cooling state
	vupc->SetPMT_Cooling = PMT_Set_Cooling;
	// add functionality to set the PMT Gain and Threshold
	vupc->SetPMT_Gain = PMT_Set_Gain;
		// add functionality to set the PMT Gain and Threshold
	vupc->SetPMT_Thresh = PMT_Set_Thresh;
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
	
	errChk( TaskControlEvent(vupc->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );

Error:

RETURN_ERR
}

static int DisplayPanels (DAQLabModule_type* mod, BOOL visibleFlag)
{
INIT_ERR

	VUPhotonCtr_type* 	vupc 	= (VUPhotonCtr_type*) mod;
	
	if (visibleFlag)
		errChk(	DisplayPanel(vupc->mainPanHndl) );
	else
		errChk( HidePanel(vupc->mainPanHndl) );

Error:
	
	return errorInfo.error;
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
			SetPanelAttribute(vupc->channels[i]->panHndl, ATTR_LEFT, (int)((chanPanWidth + MAIN_PAN_SPACING) * (nChannels-1) + MAIN_PAN_MARGIN));
			SetPanelAttribute(vupc->channels[i]->panHndl, ATTR_TOP, MAIN_PAN_MARGIN + menubarHeight);
			DisplayPanel(vupc->channels[i]->panHndl);

		}

	// reposition Task Controller panel to be the end of all channel panels
	SetPanelAttribute(vupc->taskPanHndl, ATTR_LEFT, (int)((chanPanWidth + MAIN_PAN_SPACING) * nChannels + MAIN_PAN_MARGIN));
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
		SetPanelAttribute(vupc->mainPanHndl, ATTR_WIDTH, (int)(chanPanWidth * nChannels + taskPanWidth + MAIN_PAN_SPACING * nChannels + 2 * MAIN_PAN_MARGIN));
		SetPanelAttribute(vupc->mainPanHndl, ATTR_HEIGHT, chanPanHeight + 2 * MAIN_PAN_MARGIN + menubarHeight);
		// remove scroll bars
		SetPanelAttribute(vupc->mainPanHndl, ATTR_SCROLL_BARS, VAL_NO_SCROLL_BARS);
	}

	//?
	PMTController_UpdateDisplay(vupc);
}

// resets the VUPC user interface corresponding to the resetted hardware
void ResetVUPC_UI(VUPhotonCtr_type* vupc)
{
	double 	zero		= 0.0;
	double 	twenty_mV	= 20.0;  

	for (int i = 0; i < MAX_CHANNELS; i++) {
		if(vupc->channels[i]) {
			if(i==0){
			// PMT1 is selected
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Fan,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Cooling,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Mode,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, 0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Threshold, twenty_mV);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Gain, zero);
			 
			}
			if(i==1){
			// PMT2 is selected
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Fan,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Cooling,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Mode,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, 0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Threshold, twenty_mV);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Gain, zero);
			}
			if(i==2){
			// PMT3 is selected
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Fan,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Cooling,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Mode,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, 0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Threshold, twenty_mV);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Gain, zero);
			}
			if(i==3){
			// PMT4 is selected
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Fan,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Cooling,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Mode,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_STATE1,0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_LED_TEMP1, 0);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Threshold, twenty_mV);
				SetCtrlVal (vupc->channels[i]->panHndl,VUPCChan_Gain, zero);
			}
		}
	}
}



static int PMT_Set_Mode (VUPhotonCtr_type* self, int PMTnr, PMT_Mode_type mode)
{
INIT_ERR

	errChk(PMT_SetMode (PMTnr,mode));

Error:
	
	return errorInfo.error;
}


static int PMT_Set_Fan (VUPhotonCtr_type* self, int PMTnr, BOOL value)
{
INIT_ERR

	errChk(PMT_SetFan (PMTnr,value));

Error:
	
	return errorInfo.error;
}

static int PMT_Set_Cooling (VUPhotonCtr_type* self, int PMTnr, BOOL value)
{
INIT_ERR

	errChk(PMT_SetCooling (PMTnr,value));

Error:
	
	return errorInfo.error;
}

static int PMT_Set_Gain (VUPhotonCtr_type* vupc,int PMTnr, double gain)
{
INIT_ERR

	errChk(SetPMTGain(PMTnr,gain));

Error:
	
	return errorInfo.error;
}


static int PMT_Set_Thresh (VUPhotonCtr_type* vupc,int PMTnr, double threshold)
{
INIT_ERR

	errChk(SetPMTTresh(PMTnr,threshold));

Error:
	
	return errorInfo.error;
}

int ResetActions(VUPhotonCtr_type* 	vupc)
{
INIT_ERR
	
	// set all gains to zero, all thresholds to 20 mV    
	errChk(PMTReset());
	ResetVUPC_UI(vupc);
	
	SetCtrlVal(vupc->counterPanHndl, CounterPan_TestMode, FALSE);
	PMTController_UpdateDisplay(vupc);
	
Error:
	
	return errorInfo.error;
}


static int PMTController_SetTestMode (VUPhotonCtr_type* vupc, BOOL testmode)
{
INIT_ERR

	errChk(PMT_SetTestMode(testmode));
	
Error:
	return errorInfo.error;
}

static int PMTController_Reset (VUPhotonCtr_type* vupc)
{
INIT_ERR

	errChk(ResetActions(vupc));

Error:
	return errorInfo.error;
}


static int PMTController_ResetFifo (VUPhotonCtr_type* vupc)
{
INIT_ERR

	errChk(PMTClearFifo());

Error:
	return errorInfo.error;
}


static int PMTController_UpdateDisplay (VUPhotonCtr_type* vupc)
{
INIT_ERR

	int 			i			= 0;
	BOOL 			HV			= FALSE;
	BOOL 			CurrentErr	= FALSE;
	unsigned long 	statreg		= 0;
	unsigned long 	controlreg	= 0; 

	//Get status from Hardware
	errChk(ReadPMTReg(CTRL_REG,&controlreg));
	errChk(ReadPMTReg(STAT_REG,&statreg));

	//Update UI
	// photon counter
	SetCtrlVal (vupc->counterPanHndl, CounterPan_Status, statreg);
	SetCtrlVal (vupc->counterPanHndl, CounterPan_Command, controlreg);
	SetCtrlVal (vupc->counterPanHndl, CounterPan_LEDRunning, statreg&RUNNING_BIT);
	SetCtrlVal (vupc->counterPanHndl, CounterPan_LEDFIFOAlmostFull, statreg&FFALMFULL_BIT);
	SetCtrlVal (vupc->counterPanHndl, CounterPan_LEDFIFOFull, statreg&FFOVERFLOW_BIT);
	
	
	SetCtrlVal (vupc->counterPanHndl, CounterPan_LEDDoorOpen, statreg&DOOROPEN_BIT);
	SetCtrlVal (vupc->counterPanHndl, CounterPan_LEDCounterOverflow, statreg&COVERFLOW_BIT);

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
	
	return errorInfo.error;
}




static int CVICALLBACK VUPCChannel_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	Channel_type*	chan 		= callbackData;
	int 			intval		= 0;
	float 			gain		= 0;
	float 			thresh_mV	= 0;
	float 			thresh		= 0;
	
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
					errChk(PMT_Set_Gain (chan->vupcInstance, chan->chanIdx,gain));
					break;

				case VUPCChan_Threshold:
					//get threshold value
					GetCtrlVal(panel,control,&thresh_mV);
					thresh=thresh_mV/1000; 					//convert to volts
				

					errChk(PMT_Set_Thresh (chan->vupcInstance, chan->chanIdx,thresh));
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

PRINT_ERR

	return 0;
}

static int CVICALLBACK 	VUPCSettings_CB	(int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	VUPhotonCtr_type* 	vupc 			= callbackData;
	char*				VChanName		= NULL;
	char				chanName[50] 	= "";
	int 				nsamples		= 0;
	
	switch (event) {

		case EVENT_COMMIT:

			switch (control) {

				case VUPCSet_MeasMode:
					
					int		measMode	= 0;
					GetCtrlIndex(panel, VUPCSet_MeasMode, &measMode);
					SetTaskControlMode(vupc->taskControl, measMode);
					
				//	PMTSetBufferSize(GetTaskControlMode(vupc->taskControl), vupc->samplingRate, vupc->nSamples);
					break;

				case VUPCSet_Close:

					HidePanel(vupc->acqSettingsPanHndl);
					break;
			}

			break;

		case EVENT_MARK_STATE_CHANGE:

			switch (control) {

				case VUPCSet_Channels:

					// eventData1 =  The new mark state of the list item
					// eventData2 =  0-based itemIndex of the item whose mark state is being changed

					SourceVChan_type* 	srcVChan	= NULL;
					Channel_type*		chan		= NULL;

					if (eventData1) {
						// channel checked, give new VChan name and add channel
						Fmt(chanName, "Ch. %d", eventData2+1);
						VChanName = DLVChanName((DAQLabModule_type*)vupc, vupc->taskControl, chanName, 0);
						
						// create channel
						chan = init_Channel_type(vupc, LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCChan), eventData2 + 1, VChanName);
						// rename channel mode label
						sprintf(chanName, "Ch. %d", eventData2 + 1);
						SetCtrlAttribute(chan->panHndl, VUPCChan_Mode, ATTR_LABEL_TEXT, chanName);
						// register VChan with DAQLab
						DLRegisterVChan((DAQLabModule_type*)chan->vupcInstance, (VChan_type*)chan->VChan);
						
						// connect module data and user interface callbackFn to all direct controls in the panel
						SetCtrlsInPanCBInfo(chan, ((VUPhotonCtr_type*)vupc)->uiCtrlsCB, chan->panHndl);
						
						// add sink to receive pulsetrain settings
						if (vupc->pulseTrainVChan==NULL){
							char*	pulsetrainVChanName		= DLVChanName((DAQLabModule_type*)vupc, vupc->taskControl, VChan_Default_PulseTrainSinkChan, 0);
							DLDataTypes allowedPacketTypes[] = {DL_PulseTrain_Freq, DL_PulseTrain_Ticks, DL_PulseTrain_Time};
							vupc->pulseTrainVChan= init_SinkVChan_type(pulsetrainVChanName, allowedPacketTypes, NumElem(allowedPacketTypes), chan->vupcInstance, VChanDataTimeout, NULL); 
							// register VChan with DAQLab
							DLRegisterVChan((DAQLabModule_type*)vupc, (VChan_type*)vupc->pulseTrainVChan);	
							AddSinkVChan(vupc->taskControl, vupc->pulseTrainVChan, PulseTrainDataReceivedTC, NULL); 
							free(pulsetrainVChanName);
						}
						// update main panel
						RedrawMainPanel(vupc);
						OKfree(VChanName);  


					} else {
						// channel unchecked, remove channel

						//clear corresponding control bits in control register
						PMT_ClearControl(vupc->channels[eventData2]->chanIdx);

						// get channel pointer
						srcVChan = vupc->channels[eventData2]->VChan;
						// unregister VChan from DAQLab framework
						DLUnregisterVChan((DAQLabModule_type*)vupc, (VChan_type*)srcVChan);
						// discard channel data
						discard_Channel_type(&vupc->channels[eventData2]);
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
INIT_ERR

	VUPhotonCtr_type* 	vupc 					= callbackData;
	int 				mode					= 0;
	int 				repeat					= 0;
	double 				waitBetweenIterations	= 0;

	switch (event) {
			
		case EVENT_COMMIT:
			switch (control) {
					
				case VUPCTask_Mode:
					
					GetCtrlVal(panel,control,&mode);
					SetTaskControlMode	(vupc->taskControl,mode);
					break;
					
				case VUPCTask_Repeat:
					
					GetCtrlVal(panel,control,&repeat);
					SetTaskControlIterations (vupc->taskControl, repeat+1);
					break;

				case VUPCTask_Wait:
					
					GetCtrlVal(panel,control,&waitBetweenIterations);
					SetTaskControlIterationsWait (vupc->taskControl,waitBetweenIterations);
					break;
			}
			// (re) configure Photoncounter Task Controller
			errChk( TaskControlEvent(vupc->taskControl, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
			break;
	}
	
Error:
	
PRINT_ERR

	return 0;
}

static int CVICALLBACK VUPCPhotonCounter_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR

	VUPhotonCtr_type* 	vupc 		= callbackData;
	BOOL 				testmode	= FALSE;
	int 				start		= 0;

	switch (event) {
		case EVENT_COMMIT:
			
			switch (control) {
					
				case CounterPan_FIFOReset:
					
					errChk(PMTClearFifo());
					PMTController_UpdateDisplay(vupc);
					break;
					
				case CounterPan_Reset :
					
					errChk(ResetActions(vupc));
					break;

				case CounterPan_TestMode:
					
					GetCtrlVal(panel,control,&testmode);
					errChk(PMT_SetTestMode(testmode));
					PMTController_UpdateDisplay(vupc);
					break;
					
				case CounterPan_Close:
					
					HidePanel(vupc->counterPanHndl);
					break;
			}
			break;
	}
	
Error:
	
PRINT_ERR

	return 0;
}

static BOOL ValidTaskControllerName	(char name[], void* dataPtr)
{
	return DLValidTaskControllerName(name);
}

static void SetVUPCSamplingInfo (VUPhotonCtr_type* vupc, PulseTrain_type* pulseTrain)
{
	vupc->nSamples 	= GetPulseTrainNPulses(pulseTrain);
	SetNrSamples(vupc->nSamples);
	
	switch (GetPulseTrainMode(pulseTrain)) {
				
		case PulseTrain_Finite:
				
			SetTaskControlMode(vupc->taskControl, TASK_FINITE);
			break;
				
		case PulseTrain_Continuous:
			SetTaskControlMode(vupc->taskControl, TASK_CONTINUOUS);
			break;
	}								   
		
	// assign sampling rate from pulse train info
	switch (GetPulseTrainType(pulseTrain)) {
				
		case PulseTrain_Freq:
				
			PulseTrainFreqTiming_type*	freqPulse 		= (PulseTrainFreqTiming_type*)pulseTrain;
				
			vupc->samplingRate = GetPulseTrainFreqTimingFreq(freqPulse);
			break;
				
		case PulseTrain_Time:
			
			PulseTrainTimeTiming_type*	timePulse 		= (PulseTrainTimeTiming_type*)pulseTrain;
			double 						highTime		= 0;
			double						lowTime			= 0;
	
			highTime 			= GetPulseTrainTimeTimingHighTime(timePulse);
			lowTime 			= GetPulseTrainTimeTimingLowTime(timePulse); 
			vupc->samplingRate	= 1/(highTime + lowTime);
			break;
				
		case PulseTrain_Ticks:
				
			PulseTrainTickTiming_type*	tickPulse		= (PulseTrainTickTiming_type*)pulseTrain;
			uInt32						highTicks		= 0;
			uInt32						lowTicks		= 0;
			double						tickClockFreq 	= 0;
				
			highTicks			= GetPulseTrainTickTimingHighTicks(tickPulse);
			lowTicks			= GetPulseTrainTickTimingLowTicks(tickPulse);
			tickClockFreq		= GetPulseTrainTickTimingClockFrequency(tickPulse);
			
			vupc->samplingRate	= tickClockFreq/(highTicks + lowTicks);
			break;
	} 	
						
	//update UI
	SetCtrlVal(vupc->acqSettingsPanHndl, VUPCSet_NSamples, vupc->nSamples);
	SetCtrlVal(vupc->acqSettingsPanHndl, VUPCSet_SamplingRate, vupc->samplingRate/1000);  					// display in [KHz]
	SetCtrlVal(vupc->acqSettingsPanHndl, VUPCSet_Duration, vupc->nSamples/(vupc->samplingRate * 1000));	// display in [s]
	SetCtrlIndex(vupc->acqSettingsPanHndl, VUPCSet_MeasMode, GetTaskControlMode(vupc->taskControl));     
}

void HWIterationDone(TaskControl_type* taskControl,int errorID, char errorMsg[])
{
	//hardware informs that the current iteration is done	
	TaskControlIterationDone (taskControl, errorID, errorMsg, TRUE,  NULL);	//?
}

void HWError(void)
{
	//hardware informs that an error occured during the current iteration	
}


static void VUPC_SetStepCounter	(VUPhotonCtr_type* 	vupc, size_t val)
{
	SetCtrlVal(vupc->taskPanHndl, VUPCTask_TotalIterations, (unsigned int) val);
}

void CVICALLBACK AcquisitionSettings_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	VUPhotonCtr_type* 	vupc 	=   callbackData;

	DisplayPanel(vupc->acqSettingsPanHndl);
}

void CVICALLBACK DeviceSettings_CB (int menuBar, int menuItem, void *callbackData, int panel)
{
	VUPhotonCtr_type* 	vupc 	=   callbackData;

	DisplayPanel(vupc->counterPanHndl);
}

//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	
	return 0;
}


static int UnconfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void	IterateTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortIterationFlag)
{
INIT_ERR

	VUPhotonCtr_type* 		vupc 				= GetTaskControlModuleData(taskControl);
	double 					timeout				= 3.0;
	double 					delaystep			= 0.1;
	int 					mode				= 0;
	DataPacket_type*		dataPacket			= NULL;
	DLDataTypes				dataPacketDataType	= 0;
	PulseTrain_type*    	pulseTrain			= NULL;

	// display current iteration index
	VUPC_SetStepCounter(vupc, GetCurrentIterIndex(iterator));
	
	//-------------------------------------------------------------------------------------------------------------------------------
	// Receive pulse train settings data
	//-------------------------------------------------------------------------------------------------------------------------------
	if (!IsVChanOpen((VChan_type*)vupc->pulseTrainVChan)) goto SkipPulseTrainInfo;
	
	errChk( GetDataPacket(vupc->pulseTrainVChan, &dataPacket, &errorInfo.errMsg) );
	pulseTrain = *(PulseTrain_type**)GetDataPacketPtrToData(dataPacket, &dataPacketDataType);
	
	SetVUPCSamplingInfo(vupc, pulseTrain);
		
	ReleaseDataPacket(&dataPacket);
	
	//-------------------------------------------------------------------------------------------------------------------------------
SkipPulseTrainInfo:
	
//	PMTSetBufferSize(GetTaskControlMode(vupc->taskControl), vupc->samplingRate, vupc->nSamples); 
	
	// inform that slave is armed
	errChk(SetHWTrigSlaveArmedStatus(vupc->HWTrigSlave, &errorInfo.errMsg));
	
	errChk( PMTStartAcq(GetTaskControlMode(vupc->taskControl), vupc->taskControl, vupc->samplingRate, vupc->channels) );
	

	return;
	
Error:
	
	char* msgBuff = FormatMsg(errorInfo.error, __FILE__, __func__, errorInfo.line, errorInfo.errMsg);
	TaskControlIterationDone(vupc->taskControl, errorInfo.error, msgBuff, FALSE, NULL);
	OKfree(msgBuff);
	return;
}


static int StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);

	//reset hardware iteration counter
	//ResetDataCounter();
	
	return 0;
}

static int DoneTC	(TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
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


static int StoppedTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);

	PMTStopAcq();
	PMTController_UpdateDisplay(vupc);   

	return 0;
}

static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorMsg)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	PMTStopAcq(); 
	
	return 0;
}

static int	TaskTreeStateChange (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);
	
/*	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_Command, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_Status, ATTR_DIMMED, dimmed);
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_FIFOReset, ATTR_DIMMED, dimmed);    
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_Reset, ATTR_DIMMED, dimmed);    
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_LEDDoorOpen, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_LEDRunning, ATTR_DIMMED, dimmed); 
	SetCtrlAttribute(vupc->counterPanHndl, CounterPan_TestMode, ATTR_DIMMED, dimmed);    */

	return 0;
}

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	VUPhotonCtr_type* 	vupc 	= GetTaskControlModuleData(taskControl);
	int 				error	= 0;

	error = ResetActions(vupc);

	PMTStopAcq();    
	PMTController_UpdateDisplay(vupc);

	// print error message
	DLMsg(errorMsg, 1);
}

static int ModuleEventHandler (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorMsg)
{
	VUPhotonCtr_type* 		vupc 			= GetTaskControlModuleData(taskControl);    
	
	return 0;
}

static int PulseTrainDataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR

	VUPhotonCtr_type* 	vupc				= GetTaskControlModuleData(taskControl);
	unsigned int		nSamples			= 0;
	DataPacket_type**	dataPackets			= NULL;
	size_t				nPackets			= 0;
	size_t				nElem				= 0;
	DLDataTypes			dataPacketType		= 0;  
	PulseTrain_type*    pulseTrain			= NULL;
	PulseTrainModes		pulseTrainMode		= 0;
	
	// process data only if task controller is not active
	if (taskActive) return 0; 

    // get all data packets
	errChk( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errorInfo.errMsg) );
	
	// use last data packet to set sampling info
	pulseTrain 	= *(PulseTrain_type**)GetDataPacketPtrToData(dataPackets[nPackets-1], &dataPacketType);
	
	SetVUPCSamplingInfo(vupc, pulseTrain);
	
	// release all data packets
	for (size_t i = 0; i < nPackets; i++)
		ReleaseDataPacket(&dataPackets[i]);
	
	OKfree(dataPackets);				
	
Error:	   
				
RETURN_ERR
}

static int IterationStopTC (TaskControl_type* taskControl, Iterator_type* iterator, BOOL const* abortFlag, char** errorMsg)
{
INIT_ERR
	
	VUPhotonCtr_type* 	vupc							= GetTaskControlModuleData(taskControl);
	
	// set iteration to done
	PMTStopAcq();  
	errChk( TaskControlIterationDone(vupc->taskControl, 0, "", FALSE, &errorInfo.errMsg) );
	

Error:

	
RETURN_ERR
}
 


