

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
	// Maximum number of measurement channels
#define MAX_CHANELS				4

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

#define PMT1	1
#define PMT2	2 
#define PMT3	3 
#define PMT4	4 

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

	int					chanPanHndl;

	int					statusPanHndl;

	int					settingsPanHndl;

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
	
	//added Lex (?)
	int 				statusreg;
	
	int 				controlreg;  
	


	// METHODS
			// Default NULL, functionality not implemented.
			// Override: Required, provides hardware specific movement of VUPhotonCtr.
			// Sets the Mode of the Selected PMT 
		int					(* SetPMT_Mode) 			(VUPhotonCtr_type* self, int PMTnr, PMT_Mode_type mode);
			// Sets the Fan of the Selected PMT ON or OFF
		int					(* SetPMT_Fan) 				(VUPhotonCtr_type* self, int PMTnr, BOOL value);
			// Sets the Cooling of the Selected PMT ON or OFF
		int					(* SetPMT_Cooling) 			(VUPhotonCtr_type* self, int PMTnr, BOOL value);
			// Sets the Gain and Threshold of the Selected PMT 
		int					(* SetPMT_GainThresh) 		(VUPhotonCtr_type* self, int PMTnr, double gain,double threshold);
			// Changes the status LED of the  on the VUPhotonCtr control panel
		int					(* StatusPMTLED)   			(VUPhotonCtr_type* self, int PMTnr, PMT_LED_type status);
			// Changes the temp LED of the  on the VUPhotonCtr control panel
		int					(* TempPMTLED)   			(VUPhotonCtr_type* self, int PMTnr, BOOL on_temp);
			// Updates status display of Photon Counter from structure data
		int					(* UpdateVUPhotonCtrDisplay) (VUPhotonCtr_type* self);

		void				(* DimWhenRunning)			(VUPhotonCtr_type* self, BOOL dimmed);
			// Sets the Test Mode of the PMT Controller  
		int					(* SetTestMode) 			(VUPhotonCtr_type* self, BOOL value); 
			// Resets the PMT Controller  
		int					(* ResetController) 		(VUPhotonCtr_type* self);         
			 // Resets the Fifo of thePMT Controller  
		int					(* ResetFifo) 		        (VUPhotonCtr_type* self);         
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

static int PMT_Set_Mode (VUPhotonCtr_type* self, int PMTnr, PMT_Mode_type mode);
static int PMT_Set_Fan (VUPhotonCtr_type* self, int PMTnr, BOOL value);
static int PMT_Set_Cooling (VUPhotonCtr_type* self, int PMTnr, BOOL value);
static int PMT_Set_GainThresh (VUPhotonCtr_type* self, int PMTnr, double gain,double threshold); 
static int PMT_Set_StatusLED (VUPhotonCtr_type* vupc, int PMTnr,PMT_LED_type status);
static int PMT_Set_TempLED (VUPhotonCtr_type* vupc, int PMTnr,BOOL temp_OK);
static int PMTController_UpdateDisplay (VUPhotonCtr_type* self);
void PMTController_DimWhenRunning (VUPhotonCtr_type* self, BOOL dimmed);
static int PMTController_SetTestMode(VUPhotonCtr_type* vupc,BOOL testmode);
static int PMTController_Reset(VUPhotonCtr_type* vupc);
static int PMTController_ResetFifo(VUPhotonCtr_type* vupc);  





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

	// discard channel resources
	for (int i = 0; i < MAX_CHANELS; i++)
		discard_Channel_type(&vupc->channels[i]);

	// main panel and panels loaded into it (channels and task control)
	if (vupc->mainPanHndl) {
		DiscardPanel(vupc->mainPanHndl);
		vupc->mainPanHndl = 0;
		vupc->taskPanHndl = 0;
		vupc->chanPanHndl = 0;
	}

	if (vupc->statusPanHndl)
		DiscardPanel(vupc->statusPanHndl);

	if (vupc->settingsPanHndl)
		DiscardPanel(vupc->settingsPanHndl);

	// discard DAQLabModule_type specific data
	discard_DAQLabModule(mod);
}

static Channel_type* init_Channel_type (VUPhotonCtr_type* vupcInstance, int panHndl, size_t chanIdx, char VChanName[])
{
	Channel_type* chan = malloc (sizeof(Channel_type));
	if (!chan) return NULL;

	chan->vupcInstance	= vupcInstance;
	chan->VChan			= init_SourceVChan_type(VChanName, VCHAN_USHORT, chan, NULL, NULL);
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

	if ((*chan)->panHndl) {
		DiscardPanel((*chan)->panHndl);
		(*chan)->panHndl = 0;
	}

	// discard SourceVChan
	discard_VChan_type((VChan_type**)&(*chan)->VChan);

	OKfree(*chan);  // this also removes the channel from the device structure
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
	// load Task Controller panel
	vupc->taskPanHndl		= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCTask);
	// load channel panel, the dimensions of which will be used to adjust the size of the main panel
	vupc->chanPanHndl		= LoadPanel(vupc->mainPanHndl, UI_VUPhotonCtr, VUPCChan);

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
		sprintf(buff,"Ch. %d", i);
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
	
	
	// add functionality to set PMT mode
	vupc->SetPMT_Mode = PMT_Set_Mode;
	// add functionality to set the PMT cooling fan state
	vupc->SetPMT_Fan = PMT_Set_Fan;
	// add functionality to set the PMT cooling state
	vupc->SetPMT_Cooling = PMT_Set_Cooling;
	// add functionality to set the PMT Gain and Threshold
	vupc->SetPMT_GainThresh = PMT_Set_GainThresh;
	// add functionality to change status LED        
	vupc->StatusPMTLED = PMT_Set_StatusLED;     
	// add functionality to change temp LED
	vupc->TempPMTLED = PMT_Set_TempLED;
	// Updates status display of Photon Counter
	vupc->UpdateVUPhotonCtrDisplay = PMTController_UpdateDisplay;
		// add functionality to dim and undim controls when controller is running
	vupc->DimWhenRunning	 = PMTController_DimWhenRunning;
	//add functionality to set test mode
	vupc->SetTestMode = PMTController_SetTestMode;
	//add functionality to reset PMT controller
	vupc->ResetController = PMTController_Reset;

	// redraw main panel
	RedrawMainPanel(vupc);

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
	int			menubarHeight	= 0;
	int			chanPanHeight	= 0;
	int			chanPanWidth	= 0;
	int			taskPanWidth	= 0;

	// count the number of channels in use
	for (size_t i = 0; i < MAX_CHANELS; i++)
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
	for (int i = 0; i < MAX_CHANELS; i++)
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

}

                           



                     

static int PMT_Set_Mode (VUPhotonCtr_type* vupc, int PMTnr, PMT_Mode_type mode)  
{
	int error = 0;     
	
	if(mode==PMT_MODE_ON){
		switch(PMTnr){
			case PMT1:
				vupc->controlreg=vupc->controlreg|PMT1HV_BIT|APPSTART_BIT;  //set bit
				break;
			case PMT2:
				vupc->controlreg=vupc->controlreg|PMT2HV_BIT|APPSTART_BIT;
				break;
			case PMT3:
				vupc->controlreg=vupc->controlreg|PMT3HV_BIT|APPSTART_BIT;
				break;
			case PMT4:
				vupc->controlreg=vupc->controlreg|PMT4HV_BIT|APPSTART_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				vupc->controlreg=vupc->controlreg&(~PMT1HV_BIT);   //clear bit
				break;
			case PMT2:
				vupc->controlreg=vupc->controlreg&(~PMT2HV_BIT);
				break;
			case PMT3:
				vupc->controlreg=vupc->controlreg&(~PMT3HV_BIT);
				break;
			case PMT4:
				vupc->controlreg=vupc->controlreg&(~PMT4HV_BIT);
				break;
		}
	}
		//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg);      
	
	return error;
}


static int PMT_Set_Fan (VUPhotonCtr_type* vupc, int PMTnr, BOOL value)  
{
	int error = 0; 
	
	if(value){				//Fan On
		switch(PMTnr){
			case PMT1:
				vupc->controlreg=vupc->controlreg|PMT1FAN_BIT;  //set bit
				break;
			case PMT2:
				vupc->controlreg=vupc->controlreg|PMT2FAN_BIT;
				break;
			case PMT3:
				vupc->controlreg=vupc->controlreg|PMT3FAN_BIT;
				break;
			case PMT4:
				vupc->controlreg=vupc->controlreg|PMT4FAN_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				vupc->controlreg=vupc->controlreg&(~PMT1FAN_BIT);   //clear bit
				break;
			case PMT2:
				vupc->controlreg=vupc->controlreg&(~PMT2FAN_BIT);
				break;
			case PMT3:
				vupc->controlreg=vupc->controlreg&(~PMT3FAN_BIT);
				break;
			case PMT4:
				vupc->controlreg=vupc->controlreg&(~PMT4FAN_BIT);
				break;
		}
	}
	//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg);        
	
	return error;
}

static int PMT_Set_Cooling (VUPhotonCtr_type* vupc, int PMTnr, BOOL value)  
{
	int error = 0; 
	
	if(value){			//Peltier ON
		switch(PMTnr){
			case PMT1:
				vupc->controlreg=vupc->controlreg|PMT1PELT_BIT;  //set bit
				break;
			case PMT2:
				vupc->controlreg=vupc->controlreg|PMT2PELT_BIT;
				break;
			case PMT3:
				vupc->controlreg=vupc->controlreg|PMT3PELT_BIT;
				break;
			case PMT4:
				vupc->controlreg=vupc->controlreg|PMT4PELT_BIT;
				break;
		}
	}
	else {
		switch(PMTnr){
			case PMT1:
				vupc->controlreg=vupc->controlreg&(~PMT1PELT_BIT);   //clear bit
				break;
			case PMT2:
				vupc->controlreg=vupc->controlreg&(~PMT2PELT_BIT);
				break;
			case PMT3:
				vupc->controlreg=vupc->controlreg&(~PMT3PELT_BIT);
				break;
			case PMT4:
				vupc->controlreg=vupc->controlreg&(~PMT4PELT_BIT);
				break;
		}
	}
	//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg);        
	
	return error;
}

///  HIFN  Sets the PMT gain
///  HIPAR PMTnr/PMT number,PMTGain /Gain, range ......
///  HIRET returns error, no error when 0
int SetPMTGainTresh(VUPhotonCtr_type* vupc,int PMTnr,unsigned int PMTGain,unsigned int PMTThreshold)
{
	int error=0;
	
	unsigned long combinedval;
	unsigned long THMASK=0x0000FFFF;
	unsigned long GAINMASK=0xFFFF0000;   
 
	//read control register
	error=ReadPMTReg(CTRL_REG,&vupc->controlreg);
	combinedval=((PMTGain<<16)&GAINMASK)+(PMTThreshold&THMASK);   
	
	switch(PMTnr){
		case PMT1:
			error=WritePMTReg(PMT1_CTRL_REG,combinedval);  
			//write control register
			vupc->controlreg=vupc->controlreg|UPDPMT12_BIT|APPSTART_BIT; //set bit  
		//	controlreg=controlreg&~APPSTART_BIT;  //clear bit 
			//write control register
			error=WritePMTReg(CTRL_REG,vupc->controlreg);   
			vupc->controlreg=vupc->controlreg&~UPDPMT12_BIT;  //clear bit 
			break;
		case PMT2:
			error=WritePMTReg(PMT2_CTRL_REG,combinedval);  
				//write control register
			vupc->controlreg=vupc->controlreg|UPDPMT12_BIT|APPSTART_BIT;  //set bit 
			//write control register
			error=WritePMTReg(CTRL_REG,vupc->controlreg);   
			vupc->controlreg=vupc->controlreg&~UPDPMT12_BIT;  //clear bit 
			break;
		case PMT3:
			error=WritePMTReg(PMT3_CTRL_REG,combinedval);
				//write control register
			vupc->controlreg=vupc->controlreg|UPDPMT34_BIT|APPSTART_BIT;  //set bit   
			//write control register
			error=WritePMTReg(CTRL_REG,vupc->controlreg);   
			vupc->controlreg=vupc->controlreg&~UPDPMT34_BIT;  //clear bit 
			break;
		case PMT4:
			error=WritePMTReg(PMT4_CTRL_REG,combinedval); 
			//write control register
			vupc->controlreg=vupc->controlreg|UPDPMT34_BIT|APPSTART_BIT;  //set bit   
			//write control register
			error=WritePMTReg(CTRL_REG,vupc->controlreg);   
			vupc->controlreg=vupc->controlreg&~UPDPMT34_BIT;  //clear bit 
			break;
	}
	return error;
}


int ConvertVoltsToBits(double value_in_volts)
{
	  int value_in_bits;
	  double voltsperbit=1/65535.0;
	  //1V corresponds with a bitvalue of 65535
	  value_in_bits=value_in_volts/voltsperbit;
	  return value_in_bits;
}


static int PMT_Set_GainThresh (VUPhotonCtr_type* vupc, int PMTnr, double gain,double threshold)  
{
	int error = 0;   
	unsigned int gain_in_bits;
	unsigned int threshold_in_bits;
	
	gain_in_bits=ConvertVoltsToBits(gain);
	threshold_in_bits=ConvertVoltsToBits(threshold);
	error=SetPMTGainTresh(vupc,PMTnr,gain_in_bits,threshold_in_bits);    
	
	return error;
}


static int PMTController_SetTestMode(VUPhotonCtr_type* vupc,BOOL testmode)
{
	int error=0;
	
	if (testmode) vupc->controlreg=vupc->controlreg|TESTMODE0_BIT;  //set bit
	else vupc->controlreg=vupc->controlreg&(~TESTMODE0_BIT);  //clear bit 
	//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg); 
	
	return error;
}

static int PMTController_Reset(VUPhotonCtr_type* vupc)
{
	int error=0;
	
	//set reset bit
	vupc->controlreg=vupc->controlreg|RESET_BIT;
	//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg); 
	Delay(0.01);
	//clear reset bit
	vupc->controlreg=vupc->controlreg&~RESET_BIT;
	//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg); 
	
	return error;
}


static int PMTController_ResetFifo(VUPhotonCtr_type* vupc)
{
	int error=0;
	
	//set fifo reset bit  
	vupc->controlreg=vupc->controlreg|FFRESET_BIT;
	//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg); 
	Delay(0.01);
	//clear fifo reset bit 
	vupc->controlreg=vupc->controlreg&~FFRESET_BIT; 
	//write control register
	error=WritePMTReg(CTRL_REG,vupc->controlreg); 
	
	return error;
}




static int PMT_Set_StatusLED (VUPhotonCtr_type* vupc, int PMTnr,PMT_LED_type status)
{
	int error = 0;
	int selPanHndl;   // selected panel handle
	int selCntrl;	  // selected control;

	
	selPanHndl=vupc->channels[PMTnr]->panHndl;
	selCntrl=VUPCChan_LED_STATE1;

	switch (status) {

		case PMT_LED_OFF:

			errChk( SetCtrlVal(selPanHndl, selCntrl, FALSE) );
			break;

		case PMT_LED_ON:

			errChk( SetCtrlAttribute(selPanHndl, selCntrl, ATTR_ON_COLOR, VAL_GREEN) );
			errChk( SetCtrlVal(selPanHndl, selCntrl, TRUE) );
			break;

		case PMT_LED_ERROR:

			errChk( SetCtrlAttribute(selPanHndl, selCntrl, ATTR_ON_COLOR, VAL_RED) );
			errChk( SetCtrlVal(selPanHndl, selCntrl, TRUE) );
			break;
	}

	return 0;

	Error:

	return error;
}

static int PMT_Set_TempLED (VUPhotonCtr_type* vupc, int PMTnr,BOOL temp_OK)
{
	int error = 0;
	int selPanHndl;   // selected panel handle
	int selCntrl;	  // selected control;

	selPanHndl=vupc->channels[PMTnr]->panHndl;
	selCntrl=VUPCChan_LED_TEMP1;
	
	errChk( SetCtrlVal(selPanHndl, selCntrl, temp_OK) );
	return 0;

	Error:

	return error;
}

static int PMTController_UpdateDisplay (VUPhotonCtr_type* self)
{
	int error = 0;      
	
	return error;
}

void PMTController_DimWhenRunning (VUPhotonCtr_type* self, BOOL dimmed)
{
	
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
	char				buff[DAQLAB_MAX_VCHAN_NAME + 50];
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
						DLRegisterVChan(chan->VChan);
						// update main panel
						RedrawMainPanel(vupc);


					} else {
						// channel unchecked, remove channel
						// get channel pointer
						srcVChan = vupc->channels[eventData2]->VChan;
						// unregister VChan from DAQLab framework
						DLUnregisterVChan(srcVChan);
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

