#include "UI_DataStorage.h"

//==============================================================================
//
// Title:		DataStorage.c
// Purpose:		A short description of the implementation.
//
// Created on:	12-9-2014 at 12:08:22 by Systeembeheer.
// Copyright:	IT-Groep FEW, Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
#include "DAQLab.h" 		// include this first
#include <formatio.h>
#include <userint.h>
#include "DataStorage.h"

//==============================================================================
// Constants
// Data Storage UI resource
#define UI_DataStorage			"./Modules/Data Storage/UI_DataStorage.uir"

//==============================================================================
// Types


//==============================================================================
// Module implementation

struct DatStore {

	// SUPER, must be the first member to inherit from

	DAQLabModule_type 	baseClass;

	// DATA
	
		//-------------------------
		// Task Controller
		//-------------------------
		
	TaskControl_type*	taskController;
	
			//-------------------------
		// UI
		//-------------------------

	int					mainPanHndl;
	
		//-------------------------
		// Channels
		//-------------------------

		// Array of channels of Channel_type*, the index of the array corresponding to the
		// hardware channel index. If a channel is not in use, its value in the array is NULL.

	DS_Channel_type*	channels[MAX_DS_CHANNELS];

};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions
static void	RedrawDSPanel (DataStorage_type* ds);

//-----------------------------------------
// Data Storage Task Controller Callbacks
//-----------------------------------------

//-----------------------------------------
// DataStorage Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type*	ConfigureTC				(TaskControl_type* taskControl, BOOL const* abortFlag);

static void					IterateTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag);

static void 				AbortIterationTC		(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*	StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag);

static FCallReturn_type*	DoneTC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*	StoppedTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type* 	ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag);

static void					DimTC					(TaskControl_type* taskControl, BOOL dimmed);

static void 				ErrorTC 				(TaskControl_type* taskControl, char* errorMsg);

static FCallReturn_type*	ModuleEventHandler		(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);



static int Load (DAQLabModule_type* mod, int workspacePanHndl)  ;

//==============================================================================
// Global variables

//==============================================================================
// Global functions


/// HIFN  Allocates memory and initializes the Data Storage module.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed.
DAQLabModule_type*	initalloc_DataStorage (DAQLabModule_type* mod, char className[], char instanceName[])
{
	DataStorage_type* 	ds;
	TaskControl_type*	tc;
	ListType			newTCList;
	char*				newTCName; 
	
	if (!mod) {
		ds = malloc (sizeof(DataStorage_type));
		if (!ds) return NULL;
	} else
		ds = (DataStorage_type*) mod;

	// initialize base class
	initalloc_DAQLabModule(&ds->baseClass, className, instanceName);
	
	// create Data Storage Task Controller
	tc = init_TaskControl_type (instanceName, ds, ConfigureTC, IterateTC, AbortIterationTC, StartTC, ResetTC,
								DoneTC, StoppedTC, DimTC, NULL, NULL, ModuleEventHandler, ErrorTC);
	if (!tc) {discard_DAQLabModule((DAQLabModule_type**)&ds); return NULL;}
	
	//------------------------------------------------------------

	//---------------------------
	// Parent Level 0: DAQLabModule_type
					
			// adding Task Controller to module list    
	ListInsertItem(ds->baseClass.taskControllers, &tc, END_OF_LIST); 
	newTCList = ListCreate(sizeof(TaskControl_type*));
	ListInsertItem(newTCList, &tc, END_OF_LIST);
	DLAddTaskControllers(newTCList);
	ListDispose(newTCList);
						
		// METHODS

		// overriding methods
	ds->baseClass.Discard 		= discard_DataStorage;
	ds->baseClass.Load			= Load;
	ds->baseClass.LoadCfg		= NULL; //LoadCfg;

	ds->mainPanHndl				= 0;  
	
		// DATA
	ds->taskController			= tc;
	
	for (int i = 0; i < MAX_DS_CHANNELS; i++)
		ds->channels[i] = NULL;


	//is this correct here? lex
	SetTaskControlModuleData(tc,ds);
		// METHODS
	


	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) ds;
	else
		return NULL;

}

/// HIFN Discards VUPhotonCtr_type data but does not free the structure memory.
void discard_DataStorage (DAQLabModule_type** mod)
{
	DataStorage_type* 		ds 			= (DataStorage_type*) (*mod);

	if (!ds) return;
	
	//---------------------------------------
	// discard DataStorage specific data
	//---------------------------------------
		// main panel and panels loaded into it (channels and task control)
	if (ds->mainPanHndl) {
		DiscardPanel(ds->mainPanHndl);
		ds->mainPanHndl = 0;
	}

	// discard Task Controller
	discard_TaskControl_type(&ds->taskController);

	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}


static DS_Channel_type* init_DS_Channel_type (DataStorage_type* dsInstance, int panHndl, size_t chanIdx,char VChanName[])
{
	DS_Channel_type* chan = malloc (sizeof(DS_Channel_type));
	if (!chan) return NULL;

	chan->dsInstance	= dsInstance;
	chan->VChan			= init_SourceVChan_type(VChanName, VChan_Waveform, chan, NULL, NULL);
	chan->panHndl   	= panHndl;
	chan->chanIdx		= chanIdx;   

	// register channel with device structure
	dsInstance->channels[chanIdx - 1] = chan;

	return chan;
}


static void	discard_DS_Channel_type (DS_Channel_type** chan)
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




static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	DataStorage_type* 	ds 				= (DataStorage_type*) mod;
	int error=0; 

	// load main panel
	ds->mainPanHndl 		= LoadPanel(workspacePanHndl, UI_DataStorage, DSMain);
	DisplayPanel(ds->mainPanHndl);
	//default settings:

	TaskControlEvent(ds->taskController, TASK_EVENT_CONFIGURE, NULL, NULL);


	return 0;

	Error:

	return error;

}

//-----------------------------------------
// VUPhotonCtr Task Controller Callbacks
//-----------------------------------------

static FCallReturn_type* ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	return init_FCallReturn_type(0, "", "");
}

static void IterateTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

}

static void AbortIterationTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
}

static FCallReturn_type* StartTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* DoneTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);


	return init_FCallReturn_type(0, "", "");
}
static FCallReturn_type* StoppedTC (TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);


	return init_FCallReturn_type(0, "", "");
}

static FCallReturn_type* ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return init_FCallReturn_type(0, "", "");
}

static void	DimTC (TaskControl_type* taskControl, BOOL dimmed)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
}

static void ErrorTC (TaskControl_type* taskControl, char* errorMsg)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	int error=0;


	// print error message
	DLMsg(errorMsg, 1);


}

static FCallReturn_type* ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return init_FCallReturn_type(0, "", "");
}

int CVICALLBACK CB_AddChannel (int panel, int control, int event,
							void *callbackData, int eventData1, int eventData2)
{
	SinkVChan_type* 	sinkVChan;
	DS_Channel_type*	chan;
	char*				vChanName; 
	char				buff[DAQLAB_MAX_VCHAN_NAME + 50];
	DataStorage_type* 	ds;
	int 				numitems;
	
	switch (event)
	{
		case EVENT_COMMIT:
				// channel checked, give new VChan name and add channel
						vChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
						if (!vChanName) {
							// uncheck channel
						//	CheckListItem(panel, control, eventData2, 0);
							return 0;	// action cancelled
						}
						// rename label in settings panel
						sprintf(buff, "Ch. %d: %s", eventData2+1, vChanName);
						
						GetNumListItems(panel, DSMain_Channels,&numitems);
						InsertListItem(panel, DSMain_Channels, -1, buff, numitems+1);        
						//ReplaceListItem(panel, control, eventData2, buff, eventData2+1);
						// create channel
						chan = init_DS_Channel_type(ds, panel, eventData2+1, vChanName);
						// rename channel mode label
				//		sprintf(buff, "Ch. %d", eventData2+1);
				//		SetCtrlAttribute(chan->panHndl, VUPCChan_Mode, ATTR_LABEL_TEXT, buff);
						// register VChan with DAQLab
						DLRegisterVChan((DAQLabModule_type*)chan->dsInstance, chan->VChan);

						// connect module data and user interface callbackFn to all direct controls in the panel
					//	SetCtrlsInPanCBInfo(chan, ((DataStorage_type*)ds)->uiCtrlsCB, chan->panHndl);

						// update main panel
						RedrawDSPanel(ds);

			break;
	}
	return 0;
}

int CVICALLBACK CB_RemoveChannel (int panel, int control, int event,
								  void *callbackData, int eventData1, int eventData2)
{
	SinkVChan_type* 	sinkVChan;
	DS_Channel_type*	chan; 
	DataStorage_type* 	ds;  
	char				buff[DAQLAB_MAX_VCHAN_NAME + 50];      
	
	switch (event)
	{
		case EVENT_COMMIT:
				// channel unchecked, remove channel

						// get channel pointer
						sinkVChan = ds->channels[eventData2]->VChan;
						// unregister VChan from DAQLab framework
						DLUnregisterVChan((DAQLabModule_type*)chan->dsInstance, sinkVChan);
						// discard channel data
						discard_DS_Channel_type(&ds->channels[eventData2]);
						// update channel list in the settings panel
						sprintf(buff, "Ch. %d", eventData2+1);
					//	ReplaceListItem(panel, control, eventData2, buff, eventData2+1);
					    ClearListCtrl(panel,DSMain_Channels);
						//add the channels
						
						// update main panel
						RedrawDSPanel(ds);
			break;
	}
	return 0;
}


static void	RedrawDSPanel (DataStorage_type* ds)
{
	size_t		nChannels		= 0;
	int			menubarHeight	= 0;
	int			chanPanHeight	= 0;
	int			chanPanWidth	= 0;
	int			taskPanWidth	= 0;

	// count the number of channels in use
	for (size_t i = 0; i < MAX_DS_CHANNELS; i++)
		if (ds->channels[i]) nChannels++;

    //do something?

}
