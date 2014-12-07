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

//test
char* rawfilepath="E:\\Rawdata\\";

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
	
		// Callback to install on controls from selected panel in UI_DataStorage.uir
		// Override: Optional, to change UI panel behavior. 
	CtrlCallbackPtr		uiCtrlsCB;
	
		// Callback to install on the panel from UI_DataStorage.uir
	PanelCallbackPtr	uiPanelCB;
	
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

static FCallReturn_type*	UnconfigureTC			(TaskControl_type* taskControl, BOOL const* abortFlag);

static void					IterateTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortIterationFlag);

static void 				AbortIterationTC		(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*	StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag);

static FCallReturn_type*	DoneTC					(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type*	StoppedTC				(TaskControl_type* taskControl, size_t currentIteration, BOOL const* abortFlag);

static FCallReturn_type* 	ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag);

static void					DimTC					(TaskControl_type* taskControl, BOOL dimmed);

static void 				ErrorTC 				(TaskControl_type* taskControl, char* errorMsg);

static FCallReturn_type*	DataReceivedTC			(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag);

static FCallReturn_type*	ModuleEventHandler		(TaskControl_type* taskControl, TaskStates_type taskState, size_t currentIteration, void* eventData, BOOL const* abortFlag);


static int CVICALLBACK UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
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
	tc = init_TaskControl_type (instanceName, ds, ConfigureTC, UnconfigureTC, IterateTC, AbortIterationTC, StartTC, ResetTC,
								DoneTC, StoppedTC, DimTC, NULL, ModuleEventHandler, ErrorTC);
	if (!tc) {discard_DAQLabModule((DAQLabModule_type**)&ds); return NULL;}
	
	//------------------------------------------------------------

	//---------------------------
	// Parent Level 0: DAQLabModule_type
					
						
		// METHODS

		// overriding methods
	ds->baseClass.Discard 		= discard_DataStorage;
	ds->baseClass.Load			= Load;
	ds->baseClass.LoadCfg		= NULL; //LoadCfg;

	ds->mainPanHndl				= 0; 
	
		// assign default controls callback to UI_DataStorage.uir panel
	ds->uiCtrlsCB				= UICtrls_CB;
	
		// assign default panel callback to UI_DataStorage.uir
	ds->uiPanelCB				= UIPan_CB;
	
	
		// DATA
	ds->taskController			= tc;
	
	for (int i = 0; i < MAX_DS_CHANNELS; i++)
		ds->channels[i] = NULL;


	
		// METHODS
	


	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) ds;
	else
		return NULL;

}

/// HIFN Discards DataStorage data but does not free the structure memory.
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
	DLRemoveTaskController((DAQLabModule_type*)ds, ds->taskController);
	discard_TaskControl_type(&ds->taskController);

	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}


static DS_Channel_type* init_DS_Channel_type (DataStorage_type* dsInstance, int panHndl, size_t chanIdx,char VChanName[])
{
	DLDataTypes allowedPacketTypes[] = {DL_Waveform_UShort};   	   //, WaveformPacket_UInt, WaveformPacket_Double
	
	DS_Channel_type* chan = malloc (sizeof(DS_Channel_type));
	if (!chan) return NULL;

	chan->dsInstance	= dsInstance;
	chan->VChan			= init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), chan, NULL, NULL);
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
	DataStorage_type* 	ds 			= (DataStorage_type*) mod;
	int 				error		= 0; 

	// load main panel
	ds->mainPanHndl = LoadPanel(workspacePanHndl, UI_DataStorage, DSMain);
	
	// add module's task controller to the framework
	DLAddTaskController((DAQLabModule_type*)ds, ds->taskController);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((DataStorage_type*)mod)->uiCtrlsCB, ds->mainPanHndl);
	
	
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

static FCallReturn_type* UnconfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag)
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



static void	RedrawDSPanel (DataStorage_type* ds)
{
	size_t		nChannels		= 0;
	int			menubarHeight	= 0;
	int			chanPanHeight	= 0;
	int			chanPanWidth	= 0;
	int			taskPanWidth	= 0;
	size_t 		i;

	// count the number of channels in use
	for (i= 0; i < MAX_DS_CHANNELS; i++)
		if (ds->channels[i]) nChannels++;
	
	if (nChannels>0) SetCtrlAttribute(ds->mainPanHndl,DSMain_COMMANDBUTTON_REM,ATTR_DIMMED,FALSE);
	else SetCtrlAttribute(ds->mainPanHndl,DSMain_COMMANDBUTTON_REM,ATTR_DIMMED,TRUE); 
	

}

static int CVICALLBACK UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
		DataStorage_type* 	ds 			= callbackData;
	
	return 0;
}


static int CVICALLBACK UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
	DataStorage_type* 	ds 			= callbackData;
	SinkVChan_type* 	sinkVChan;
	DS_Channel_type*	chan; 
	char				buff[DAQLAB_MAX_VCHAN_NAME + 50];  
	char*				vChanName;
	int 				numitems;
	int 				checked;
	int 				i;
	int 				channr=0;
	int					itemIndex;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					case DSMain_Channels: 
					break;
					
					case DSMain_COMMANDBUTTON_ADD:
						do {
							channr++;	
						}
						while ((ds->channels[channr-1]!=NULL)&&(channr<=MAX_DS_CHANNELS));
						if (channr>MAX_DS_CHANNELS) return 0;  //no more channels available
						
						// channel checked, give new VChan name and add channel
						vChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
						if (!vChanName) {
							return 0;	// action cancelled
						}
						// rename label in settings panel
						sprintf(buff, "%s", vChanName);
						GetNumListItems(panel, DSMain_Channels,&numitems);
						InsertListItem(panel, DSMain_Channels, -1, buff,channr);        
					
						// create channel
						chan = init_DS_Channel_type(ds, panel, channr, vChanName);
		
						// register VChan with DAQLab
						DLRegisterVChan((DAQLabModule_type*)ds, (VChan_type*)chan->VChan);
						AddSinkVChan(ds->taskController, chan->VChan, DataReceivedTC, TASK_VCHAN_FUNC_NONE);

						// update main panel
						RedrawDSPanel(ds);
					break;
					
					case DSMain_COMMANDBUTTON_REM:
						GetNumListItems(panel, DSMain_Channels,&numitems);    
						for(i=0;i<numitems;i++){
							IsListItemChecked(panel,DSMain_Channels, i, &checked);      
							if (checked==1) {
					   			// get channel pointer
								sinkVChan = ds->channels[i]->VChan;
								// unregister VChan from DAQLab framework
								DLUnregisterVChan((DAQLabModule_type*)ds, (VChan_type*)sinkVChan);
								// discard channel data
								GetValueFromIndex (panel, DSMain_Channels, i, &channr);
								discard_DS_Channel_type(&ds->channels[channr]);
								// update channel list 
							//	DeleteListItem(panel,DSMain_Channels ,channr , 1);
							}
						}		 
						// update main panel
						RedrawDSPanel(ds);
					break;
					
			}
	}
	return 0;
}


static FCallReturn_type* DataReceivedTC	(TaskControl_type* taskControl, TaskStates_type taskState, SinkVChan_type* sinkVChan, BOOL const* abortFlag)
{
	
	DataStorage_type*	ds					= GetTaskControlModuleData(taskControl);
	FCallReturn_type*	fCallReturn			= NULL;
	unsigned int		nSamples;
	int					error;
	DataPacket_type**	dataPackets			= NULL;
	unsigned short int* shortDataPtr		= NULL;
	size_t				nPackets;
	size_t				nElem;
	char*				VChanName			= GetVChanName((VChan_type*)sinkVChan);
	char* 				rawfilename; 
	int 				filehandle;
	int 				elementsize=2;
	char* 				errMsg 				= StrDup("Writing data to ");
	char				cmtStatusMessage[CMT_MAX_MESSAGE_BUF_SIZE];
	void*				dataPacketDataPtr;
	DLDataTypes			dataPacketType;  
			
			
	
	switch(taskState) {
			
		case TASK_STATE_UNCONFIGURED:			
		case TASK_STATE_CONFIGURED:						
		case TASK_STATE_INITIAL:							
		case TASK_STATE_IDLE:
		case TASK_STATE_STOPPING:
		case TASK_STATE_DONE:
		case TASK_STATE_RUNNING_WAITING_HWTRIG_SLAVES:
		case TASK_STATE_RUNNING:
		case TASK_STATE_RUNNING_WAITING_ITERATION:
			
			
			// get all available data packets
			if ((fCallReturn = GetAllDataPackets(sinkVChan, &dataPackets, &nPackets))) goto Error;
			
			rawfilename=malloc(MAXCHAR*sizeof(char));
			Fmt (rawfilename, "%s<%srawdata_%s.bin", rawfilepath,VChanName); 
				
			for (size_t i = 0; i < nPackets; i++) {
				dataPacketDataPtr = GetDataPacketPtrToData(dataPackets[i], &dataPacketType);
				switch (dataPacketType) {
					case DL_Waveform_UShort:
						shortDataPtr = GetWaveformDataPtr(*(Waveform_type**)dataPacketDataPtr, &nElem);
						
						//test
			
						//	ArrayToFile(rawfilename,shortDataPtr,VAL_SHORT_INTEGER,nElem,1,VAL_GROUPS_TOGETHER,VAL_GROUPS_AS_COLUMNS,VAL_SEP_BY_TAB,0,VAL_BINARY,VAL_APPEND);
						filehandle=OpenFile (rawfilename, VAL_WRITE_ONLY, VAL_APPEND, VAL_BINARY);
						if (filehandle<0) {
							AppendString(&errMsg, VChanName, -1); 
							AppendString(&errMsg, " failed. Reason: file open failed", -1); 
							fCallReturn = init_FCallReturn_type(-1, "DataStorage", errMsg);
						}
						error=WriteFile (filehandle, shortDataPtr, nElem*elementsize);
						if (error<0) {
							AppendString(&errMsg, VChanName, -1); 
							AppendString(&errMsg, " failed. Reason: file write failed", -1); 
							fCallReturn = init_FCallReturn_type(-1, "DataStorage", errMsg);
						}
						CloseFile(filehandle);
					break;
				}
				ReleaseDataPacket(&dataPackets[i]);
			}
		
			free(rawfilename); 
			OKfree(dataPackets);				
			break;
				
			
		case TASK_STATE_ERROR:
			
			ReleaseAllDataPackets(sinkVChan);
			
			break;
	}
	
	return init_FCallReturn_type(0, "", "");
	
	
Error:
	
	OKfree(VChanName);
	OKfree(errMsg);
	return fCallReturn;
}
