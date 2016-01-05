//==============================================================================
//
// Title:		DataStorage.c
// Purpose:		A short description of the implementation.
//
// Created on:	12-9-2014 at 12:08:22 by Lex van der Gracht.
// Copyright:	Vrije Universiteit Amsterdam. All Rights Reserved.
//
//==============================================================================

//==============================================================================
// Include files
//#include "HDF5support.h"
#include "DAQLab.h" 		// include this first
#include "DAQLabUtility.h"
#include "pathctrl.h"
#include "UI_DataStorage.h"
#include <formatio.h>
#include <userint.h>
#include "DataStorage.h"
#include "HDF5support.h"




//==============================================================================
// Constants
// Data Storage UI resource
#define UI_DataStorage			"./Framework/Data Storage/UI_DataStorage.uir"
#define MAXBASEFILEPATH			MAX_PATHNAME_LEN

//test
#define DATAFILEBASEPATH 		"C:\\Rawdata\\"
#define VChanDataTimeout							1e4					// Timeout in [ms] for Sink VChans to receive data  


//==============================================================================
// Types


//==============================================================================
// Module implementation

struct DataStorage {

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
	
	char*				basefilepath;
	char*				rawDataPath;    // test, for saving raw data
	char*				hdf5DataFileName;   // name of hdf5 data file 
	BOOL				overwrite_files;
	
		// Callback to install on controls from selected panel in UI_DataStorage.uir
		// Override: Optional, to change UI panel behavior. 
	CtrlCallbackPtr		uiCtrlsCB;
	
		// Callback to install on the panel from UI_DataStorage.uir
	PanelCallbackPtr	uiPanelCB;
	
		//-------------------------
		// Channels
		//-------------------------
		// Available datastorage channels. Of DS_Channel_type* 
	ListType			channels;
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions

static void					RedrawDSPanel 			(DataStorage_type* ds);

static int 					DataReceivedTC 			(TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg);

//-----------------------------------------
// Data Storage Task Controller Callbacks
//-----------------------------------------
static int					TaskTreeStateChange 	(TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg);
static void				 	ErrorTC 				(TaskControl_type* taskControl, int errorID, char* errorMsg);
//-----------------------------------------


static int CVICALLBACK 		UIPan_CB 				(int panel, int event, void *callbackData, int eventData1, int eventData2);
static int CVICALLBACK 		UICtrls_CB 				(int panel, int control, int event, void *callbackData, int eventData1, int eventData2);
static int 					Load 					(DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg);

//==============================================================================
// Global variables

//==============================================================================
// Global functions


/// HIFN  Allocates memory and initializes the Data Storage module.
/// HIPAR mod/ if NULL both memory allocation and initialization must take place.
/// HIPAR mod/ if !NULL the function performs only an initialization of a DAQLabModule_type.
/// HIRET address of a DAQLabModule_type if memory allocation and initialization took place.
/// HIRET NULL if only initialization was performed.
DAQLabModule_type*	initalloc_DataStorage (DAQLabModule_type* mod, char className[], char instanceName[], int workspacePanHndl)
{
	DataStorage_type* 	ds		= NULL;
	TaskControl_type*	tc		= NULL;
	
	if (!mod) {
		ds = malloc (sizeof(DataStorage_type));
		if (!ds) return NULL;
	} else
		ds = (DataStorage_type*) mod;

	// initialize base class
	initalloc_DAQLabModule(&ds->baseClass, className, instanceName, workspacePanHndl);
	
	ds->basefilepath		= StrDup(DATAFILEBASEPATH);
	ds->rawDataPath 		= NULL;
	ds->hdf5DataFileName		= NULL;
	ds->overwrite_files		= FALSE;
	
	// create Data Storage Task Controller
	tc = init_TaskControl_type (instanceName, ds, DLGetCommonThreadPoolHndl(), NULL, NULL, NULL, NULL, NULL,
								 NULL, NULL, NULL, TaskTreeStateChange, NULL, NULL, ErrorTC);
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
	
	if (!(ds->channels			= ListCreate(sizeof(DS_Channel_type*))))	return NULL; 
	

	

	
		// METHODS
	


	//----------------------------------------------------------
	if (!mod)
		return (DAQLabModule_type*) ds;
	else
		return NULL;

}

static DS_Channel_type* init_DS_Channel_type (DataStorage_type* ds, int panHndl, char VChanName[])
{
	DLDataTypes allowedPacketTypes[] = { DL_Waveform_Char,						
										 DL_Waveform_UChar,						
										 DL_Waveform_Short,						
										 DL_Waveform_UShort,					
										 DL_Waveform_Int,
										 DL_Waveform_UInt,
										 DL_Waveform_Int64,
										 DL_Waveform_UInt64,
										 DL_Waveform_SSize,						
										 DL_Waveform_Size,
										 DL_Waveform_Float,						
									 	 DL_Waveform_Double,
									     DL_Image };   	   
	
	DS_Channel_type* chan = malloc (sizeof(DS_Channel_type));
	if (!chan) return NULL;

	chan->dsInstance	= ds;
	chan->VChan			= init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), chan, VChanDataTimeout, NULL);
	chan->panHndl   	= panHndl;

	// add new channel to data storage module list of channels
	ListInsertItem(ds->channels, &chan, END_OF_LIST);


	return chan;
}


static void	discard_DS_Channel_type (DS_Channel_type** chan)
{   
	if (!*chan) return;
	
	DS_Channel_type** 	chanPtr			= NULL;
	DataStorage_type* 	ds				= (*chan)->dsInstance;
	size_t				nchannels		= ListNumItems(ds->channels);
	size_t				chIdx			= 1;
	char*				vchanname		= NULL;
	char*				discvchanname	= NULL; 
	
	// remove channel     
	for (size_t i = 1; i <= nchannels; i++) {	
		chanPtr = ListGetPtrToItem(ds->channels, i);
		vchanname=GetVChanName((VChan_type*)(*chanPtr)->VChan);
		discvchanname=GetVChanName((VChan_type*)(*chan)->VChan);
		if (strcmp(discvchanname, vchanname)==0){ 
			// remove from framework if vchan names match
			// unregister VChan from DAQLab framework
			DLUnregisterVChan((DAQLabModule_type*)ds, (VChan_type*)(*chanPtr)->VChan);
			// discard channel data structure
			ListRemoveItem(ds->channels, 0, chIdx);
			free(vchanname);
			free(discvchanname);
			break;
		}
		else {
			free(vchanname);
			free(discvchanname);
		}
		chIdx++;
	}

	// discard SourceVChan
	discard_VChan_type((VChan_type**)&(*chan)->VChan);
	
	

	OKfree(*chan);  // this also removes the channel from the device structure
}


/// HIFN Discards DataStorage data but does not free the structure memory.
void discard_DataStorage (DAQLabModule_type** mod)
{
	DataStorage_type* 		ds 		= (DataStorage_type*) (*mod);
	
	if (!ds) return;
	
	//---------------------------------------
	// discard DataStorage specific data
	//---------------------------------------
	// main panel and panels loaded into it (channels and task control)
	
	OKfreePanHndl(ds->mainPanHndl);
	
	// discard Task Controller
	DLRemoveTaskController((DAQLabModule_type*)ds, ds->taskController);
	discard_TaskControl_type(&ds->taskController);

	// ListDispose (offsetlist); 
	OKfreeList(&ds->channels, (DiscardFptr_type)discard_DS_Channel_type);
	
	OKfree(ds->basefilepath);
	OKfree(ds->rawDataPath); 
	OKfree(ds->hdf5DataFileName);  
	
	//----------------------------------------
	// discard DAQLabModule_type specific data
	//----------------------------------------
	
	discard_DAQLabModule(mod);
}


//creates a usable data dir, returns nonzero if failed
int CreateRawDataDir(DataStorage_type* ds, TaskControl_type* taskControl)
{
	char*			  rootname			= NULL; 
	int 			  err				= 0;
	ssize_t 		  fileSize			= 0;
	char*			  pathName			= NULL;
	BOOL			  uniquedir			= FALSE;
	int				  dircounter		= 0;
	char*			  dirctrstr			= NULL;
	TaskControl_type* root_tc			= NULL; 
	
	
	if (!taskControl) return -1;
	
	//determine dir name  
	root_tc		= GetTaskControlRootParent (taskControl);
	rootname	= GetTaskControlName(root_tc);

	OKfree(ds->rawDataPath);
	ds->rawDataPath=StrDup(ds->basefilepath);
	AppendString(&ds->rawDataPath,"\\",-1);
	AppendString(&ds->rawDataPath,rootname,-1);
	free(rootname);

	
	if (FileExists (ds->rawDataPath, &fileSize)){
		if(ds->overwrite_files){
		//clear dir
			pathName=StrDup(ds->rawDataPath);
			AppendString(&pathName, "*.*", -1);	//create file name
			DeleteFile(pathName); 
			free(pathName);
		}
		else {
		//create unique dir
			uniquedir=FALSE;
			while (!uniquedir){
				pathName=StrDup(ds->rawDataPath); 
				dircounter++;
				dirctrstr=malloc(MAXBASEFILEPATH*sizeof(char));
				Fmt(dirctrstr,"%s<_%i",dircounter);
				AppendString(&pathName,dirctrstr,-1);
				OKfree(dirctrstr); 
				if (FileExists (pathName, &fileSize)!=1) {
					uniquedir=TRUE;
					OKfree(ds->rawDataPath);    
					ds->rawDataPath=StrDup(pathName);
					err=MakeDir(ds->rawDataPath);
				}
				free(pathName);
			}
			
		}
	} else  
		err = MakeDir(ds->rawDataPath);  
	
	return err;
	
}


char* CreateFullIterName(Iterator_type*	currentiter)
{
	
	Iterator_type* 	  childiter		= NULL;
	Iterator_type* 	  parentiter	= NULL;
	char*			  name			= NULL;
	char*			  fullname		= NULL; 
	char*			  tcname		= NULL;
	size_t			  iteridx		= 0;
	
	if (currentiter==NULL) return NULL;
	
	parentiter = GetIteratorParent(currentiter);
	if (!parentiter) return NULL;  //no parent
	
	tcname		= GetCurrentIterName(parentiter);
	iteridx		= GetCurrentIterIndex(parentiter);
	fullname	= malloc(MAXBASEFILEPATH*sizeof(char));  
	Fmt (fullname, "%s<%s[w3]#%i",tcname,iteridx); 
	OKfree(tcname);
	
	while (parentiter) {  
		childiter	= parentiter;
		parentiter	= GetIteratorParent(childiter);
		if (!parentiter) return fullname;  //no parent, return current name
		
		tcname	= GetCurrentIterName(parentiter);
		iteridx	= GetCurrentIterIndex(parentiter);
		name	= malloc(MAXBASEFILEPATH*sizeof(char));
		Fmt (name, "%s<%s[w3]#%i",tcname,iteridx);    		
		AddStringPrefix (&fullname,"/",-1);    
		AddStringPrefix (&fullname,name,-1);
		OKfree(tcname);
		OKfree(name);
	}
	
	return fullname;	
}

static int Load (DAQLabModule_type* mod, int workspacePanHndl, char** errorMsg)
{
INIT_ERR

	DataStorage_type* 	ds 			= (DataStorage_type*) mod;
	
	// load main panel
	ds->mainPanHndl = LoadPanel(workspacePanHndl, UI_DataStorage, DSMain);
	
	// add module's task controller to the framework
	DLAddTaskController((DAQLabModule_type*)ds, ds->taskController);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((DataStorage_type*)mod)->uiCtrlsCB, ds->mainPanHndl);
	
	SetCtrlVal(ds->mainPanHndl,DSMain_STRING,ds->basefilepath);
	
	
	DisplayPanel(ds->mainPanHndl);
	//default settings:

	errChk( TaskControlEvent(ds->taskController, TC_Event_Configure, NULL, NULL, &errorInfo.errMsg) );
	
Error:
	
RETURN_ERR
}


//-----------------------------------------
// DataStorage Task Controller Callbacks
//-----------------------------------------

static int TaskTreeStateChange (TaskControl_type* taskControl, TaskTreeStates state, char** errorMsg)
{
INIT_ERR
	
	DataStorage_type* 	ds 			= GetTaskControlModuleData(taskControl);
	size_t				nChans		= ListNumItems(ds->channels);
	DS_Channel_type*	dsChan		= NULL;
	BOOL				storeData	= FALSE;
	
	// check if there is at least one open VChan to receive data
	for (size_t i = 1; i <= nChans; i++) {
		dsChan = *(DS_Channel_type**)ListGetPtrToItem(ds->channels, i);
		if (IsVChanOpen((VChan_type*)dsChan->VChan)) {
			storeData = TRUE;
			break;
		}
	}
	
	if (state && storeData) {
		
		// create a new data directory
		OKfree(ds->rawDataPath);			    // free previous path name
		CreateRawDataDir(ds, taskControl);
		
		// create HDF5 file
		OKfree(ds->hdf5DataFileName);               //free previous data file name
		nullChk( ds->hdf5DataFileName = malloc(MAX_PATH * sizeof(char)) ); 
		Fmt(ds->hdf5DataFileName,"%s<%s\\data.h5", ds->rawDataPath);
		errChk( CreateHDF5File(ds->hdf5DataFileName, "/dset", &errorInfo.errMsg) );
	}
	
	return 0;
	
Error:
	
	// cleanup
	OKfree(ds->rawDataPath);
	OKfree(ds->hdf5DataFileName);
	
RETURN_ERR
}

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	// print error message
	DLMsg(errorMsg, 1);
}
 
static void	RedrawDSPanel (DataStorage_type* ds)
{
	size_t		nChannels		= 0;
	
	// count the number of channels in use
	if (ds->channels) nChannels = ListNumItems(ds->channels); 
	
	if (nChannels > 0) SetCtrlAttribute(ds->mainPanHndl,DSMain_RemoveChan,ATTR_DIMMED,FALSE);
	else SetCtrlAttribute(ds->mainPanHndl,DSMain_RemoveChan,ATTR_DIMMED,TRUE); 
	

}

static int CVICALLBACK UIPan_CB (int panel, int event, void *callbackData, int eventData1, int eventData2)
{
	//DataStorage_type* 	ds 			= callbackData;
	
	return 0;
}


static int CVICALLBACK UICtrls_CB (int panel, int control, int event, void *callbackData, int eventData1, int eventData2)
{
INIT_ERR
	
	DataStorage_type* 		ds 									= callbackData;
	DS_Channel_type*		chan								= NULL; 
	DS_Channel_type**		chanPtr								= NULL; 
	char					buff[DAQLAB_MAX_VCHAN_NAME + 50]	= "";  
	char*					vChanName							= NULL;
	char					channame[DAQLAB_MAX_VCHAN_NAME]		= "";
	int 					numItems							= 0;
	int 					checked								= 0;
	int 					treeindex							= 0;
	int 					i									= 0;
	int 					channr								= 0;
	int 					reply								= 0;
	char*					currentbasepath						= NULL;
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					
				case DSMain_Channels: 
					
					break;
					
				case DSMain_AddChan:
					
					// channel checked, give new VChan name and add channel
					vChanName = DLGetUINameInput("New Virtual Channel", DAQLAB_MAX_VCHAN_NAME, DLValidateVChanName, NULL);
					if (!vChanName) return 0;	// action cancelled
					
					// rename label in settings panel
					sprintf(buff, "%s", vChanName);
					InsertListItem(panel, DSMain_Channels, -1, buff, channr); 
					GetNumListItems(panel, DSMain_Channels, &numItems);
					CheckListItem(panel, DSMain_Channels, numItems - 1, TRUE); 
					
					// create channel
					chan = init_DS_Channel_type(ds, panel, vChanName);
		
					// register VChan with DAQLab
					errChk( AddSinkVChan(ds->taskController, chan->VChan, DataReceivedTC, &errorInfo.errMsg) );
					DLRegisterVChan((DAQLabModule_type*)ds, (VChan_type*)chan->VChan);
					
					// update main panel
					RedrawDSPanel(ds);
					free(vChanName);
					break;
					
				case DSMain_RemoveChan:
					
					GetNumListItems(panel, DSMain_Channels,&numItems);    
					
					for(treeindex = 0;treeindex < numItems; treeindex++){
						IsListItemChecked(panel,DSMain_Channels,treeindex, &checked);      
						if (checked == 1) {
					   		// get channel name
							GetLabelFromIndex (panel, DSMain_Channels, treeindex, channame); 
							//get channel from list
							if (ds->channels) {
								numItems = ListNumItems(ds->channels); 
								for (i = 1; i <= numItems; i++) {
									chanPtr = ListGetPtrToItem(ds->channels, i);
									if (chanPtr!=NULL) 
										chan=*chanPtr;
										
									if (chan) {
										if (!strcmp(GetVChanName((VChan_type*)chan->VChan), channame)){   
										// strings are equal; channel found
										// discard channel data
										discard_DS_Channel_type(&chan);
										// update channel list 
										DeleteListItem(panel,DSMain_Channels ,treeindex , 1);
										return 0;
										}
									}
								}
							}
						}
					}
					// update main panel
					RedrawDSPanel(ds);
					break;
					
				case DSMain_Change:
					
					currentbasepath = malloc(MAX_PATHNAME_LEN*sizeof(char));
					GetCtrlVal(ds->mainPanHndl,DSMain_STRING,currentbasepath);
					reply=DirSelectPopup (currentbasepath, "Select Base File Folder:", 1, 1, ds->basefilepath);
					if (reply) SetCtrlVal(ds->mainPanHndl,DSMain_STRING,ds->basefilepath);
					free(currentbasepath);
					break;
					
				case DSMain_STRING: 
					
						 //indicator only
					break;
					
				case DSMain_CHECKBOX_OVERWRITE:
					
					GetCtrlVal(panel,control,&ds->overwrite_files);
					break;
					
			}
			break;
			
		case EVENT_MARK_STATE_CHANGE:
			
			switch (control) {
					
				case DSMain_Channels:
					
					chan = *(DS_Channel_type**)ListGetPtrToItem(ds->channels, eventData2 + 1);
					
					if (eventData1)
						// channels was activated
						SetVChanActive((VChan_type*)chan->VChan, TRUE);
					else
						// channel was deactivated
						SetVChanActive((VChan_type*)chan->VChan, FALSE);
					
					break;
			}
			
			break;
			
	}
	
Error:
	
PRINT_ERR

	return 0;
}

static int DataReceivedTC (TaskControl_type* taskControl, TCStates taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorMsg)  
{
INIT_ERR
	
	DataStorage_type*		ds						= GetTaskControlModuleData(taskControl);
	DataPacket_type**		dataPackets				= NULL;
	size_t					nPackets				= 0;
	char*					sinkVChanName			= GetVChanName((VChan_type*)sinkVChan);
	SourceVChan_type*   	sourceVChan				= GetSourceVChan(sinkVChan); 
	char*					sourceVChanName			= GetVChanName((VChan_type*)sourceVChan);  
	
	void*					dataPacketDataPtr		= NULL;
	DLDataTypes				dataPacketType			= 0; 
	size_t 					i						= 0;
	DSInfo_type*			dsInfo					= NULL;
	
	// get all available data packets
	errChk( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errorInfo.errMsg) );
	
	for (i = 0; i < nPackets; i++) {
		if (!dataPackets[i] || !ds->hdf5DataFileName || !ds->hdf5DataFileName[0]) {
			
			ReleaseDataPacket(&dataPackets[i]); 
			
		} else {
			
			dataPacketDataPtr 	= GetDataPacketPtrToData(dataPackets[i], &dataPacketType); 
			dsInfo				= GetDataPacketDSData(dataPackets[i]);
			
			switch (dataPacketType) {
					
					case DL_Waveform_Char:
					case DL_Waveform_UChar:
					case DL_Waveform_Short:
					case DL_Waveform_UShort:
					case DL_Waveform_Int:
					case DL_Waveform_UInt:
					case DL_Waveform_Int64:
					case DL_Waveform_UInt64:
					case DL_Waveform_Float:
					case DL_Waveform_Double:
						
						errChk( WriteHDF5Waveform(ds->hdf5DataFileName, sourceVChanName, dsInfo, *(Waveform_type**)dataPacketDataPtr, Compression_GZIP, &errorInfo.errMsg) );  
						break;
					
					case DL_Image:
						
						errChk( WriteHDF5Image(ds->hdf5DataFileName, sourceVChanName, dsInfo, *(Image_type**)dataPacketDataPtr, Compression_GZIP, &errorInfo.errMsg) );
						break;
						
					default:
						
						// not implemented
						break;
			}
			
			ReleaseDataPacket(&dataPackets[i]);
		}
	}
		
Error:
	
	OKfree(dataPackets);
	OKfree(sinkVChanName);
	OKfree(sourceVChanName);
	
RETURN_ERR
}


