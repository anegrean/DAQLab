#include "nivision.h"
#include "pathctrl.h"
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
#define MAXBASEFILEPATH			MAX_PATHNAME_LEN

//test
#define  DATAFILEBASEPATH 		"C:\\Rawdata\\"
#define VChanDataTimeout							1e4					// Timeout in [ms] for Sink VChans to receive data  


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
	
	char*				basefilepath;
	char*				rawdatapath;    //test, for saving raw data
	char*				name;			//test, storage name;
	BOOL				overwrite_files;
	
		// Callback to install on controls from selected panel in UI_DataStorage.uir
		// Override: Optional, to change UI panel behavior. 
	CtrlCallbackPtr		uiCtrlsCB;
	
		// Callback to install on the panel from UI_DataStorage.uir
	PanelCallbackPtr	uiPanelCB;
	
		//-------------------------
		// Channels
		//-------------------------
		// Available datastorage channel data. Of DS_Channel_type* 
	ListType					channels;
	
};

//==============================================================================
// Static global variables

//==============================================================================
// Static functions
static void	RedrawDSPanel (DataStorage_type* ds);

static int DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo);

//-----------------------------------------
// Data Storage Task Controller Callbacks
//-----------------------------------------
//static int 					ConfigureTC 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
//static int 					UnConfigureTC 			(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 
//datastorage module shouldn't iterate
//static void					IterateTC				(TaskControl_type* taskControl, BOOL const* abortIterationFlag);
//static void 				AbortIterationTC		(TaskControl_type* taskControl, BOOL const* abortFlag);
//static int					StartTC					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
//static int					DoneTC					(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
//static int					StoppedTC				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo);
static void					DimUITC					(TaskControl_type* taskControl, BOOL dimmed);
//static void					TCActive				(TaskControl_type* taskControl, BOOL UITCActiveFlag);
static int				 	ResetTC 				(TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo); 
static void				 	ErrorTC 				(TaskControl_type* taskControl, int errorID, char* errorMsg);
//static int					ModuleEventHandler		(TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, void* eventData, BOOL const* abortFlag, char** errorInfo); 
//-----------------------------------------
// DataStorage Task Controller Callbacks
//-----------------------------------------




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
	
	ds->basefilepath		= StrDup(DATAFILEBASEPATH);
	ds->rawdatapath 		= NULL;
	ds->name				= malloc(MAXBASEFILEPATH*sizeof(char)); 
	ds->overwrite_files		= FALSE;
	
	// create Data Storage Task Controller
	tc = init_TaskControl_type (instanceName, ds, NULL, NULL, NULL, NULL,NULL , ResetTC,
								NULL, NULL, DimUITC, NULL, NULL, ErrorTC);
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
	
	if (ds->channels) {
		size_t 				nItems = ListNumItems(ds->channels);
		DS_Channel_type**   channelPtr;
		for (size_t i = 1; i <= nItems; i++) {
			channelPtr = ListGetPtrToItem(ds->channels, i);
			(*(*channelPtr)->Discard)	(channelPtr); 
		}
		
		ListDispose(ds->channels);
	}
	
	discard_DAQLabModule(mod);
}



static DS_Channel_type* init_DS_Channel_type (DataStorage_type* ds, int panHndl, char VChanName[])
{
	DLDataTypes allowedPacketTypes[] = {DL_Waveform_UShort,DL_Image_NIVision};   	   //, WaveformPacket_UInt, WaveformPacket_Double
	
	DS_Channel_type* chan = malloc (sizeof(DS_Channel_type));
	if (!chan) return NULL;

	chan->dsInstance	= ds;
	chan->VChan			= init_SinkVChan_type(VChanName, allowedPacketTypes, NumElem(allowedPacketTypes), chan,VChanDataTimeout, NULL, NULL);
	chan->panHndl   	= panHndl;

	// add new channel to data storage module list of channels
	ListInsertItem(ds->channels, &chan, END_OF_LIST);


	return chan;
}


static void	discard_DS_Channel_type (DS_Channel_type** chan)
{   
		
	DS_Channel_type** 	chanPtr;
	DataStorage_type* 	ds				= (*chan)->dsInstance;
	size_t				nchannels		= ListNumItems(ds->channels);
	size_t				chIdx			= 1;
	int 				i;
	
	if (!*chan) return;

//	if ((*chan)->panHndl) {
//		DiscardPanel((*chan)->panHndl);
//		(*chan)->panHndl = 0;
//	}
	
	// remove channel     
	for (size_t i = 1; i <= nchannels; i++) {	
		chanPtr = ListGetPtrToItem(ds->channels, i);
		if (strcmp(GetVChanName((*chan)->VChan),GetVChanName((*chanPtr)->VChan))==0){ 
			// remove from framework if vchan names match
			// unregister VChan from DAQLab framework
			DLUnregisterVChan((DAQLabModule_type*)ds, (VChan_type*)(*chanPtr)->VChan);
			// discard channel data structure
			ListRemoveItem(ds->channels, 0, chIdx);
			break;
		}
		chIdx++;
	}

	// discard SourceVChan
	discard_VChan_type((VChan_type**)&(*chan)->VChan);
	
	

	OKfree(*chan);  // this also removes the channel from the device structure
}




 //creates a usable data dir, returns nonzero if failed
int CreateRawDataDir(DataStorage_type* 	ds,TaskControl_type* taskControl)
{
	
	TaskControl_type* child;
	TaskControl_type* parent;
	char*			  name;
	char*			  fullname; 
	char*			  tcname;
	char*			  rawdatapath;
	int 			  err=0;
	ssize_t 		  fileSize;
	char*			  pathName;
	BOOL			  uniquedir;
	int				  dircounter=0;
	char*			  dirctrstr;
	
	
	
	if (taskControl==NULL) return -1;
	
	
	parent=GetTaskControlParent(taskControl);
	if (parent==NULL) return -1;  //no parent
	tcname=GetTaskControlName(parent);
	fullname=malloc(MAXBASEFILEPATH*sizeof(char));  
	Fmt (fullname, "%s<%s",tcname); 
	OKfree(tcname); 
	while (parent!=NULL) {  
		child=parent;
		parent=GetTaskControlParent(child);
		if (parent==NULL) break;
		tcname=GetTaskControlName(parent);
		name=malloc(MAXBASEFILEPATH*sizeof(char));
		Fmt (name, "%s<%s",tcname);    		
		AddStringPrefix (&fullname,"_",-1);    
		AddStringPrefix (&fullname,name,-1);
		OKfree(name);
		OKfree(tcname); 
	}
	OKfree(ds->rawdatapath);
	ds->rawdatapath=StrDup(ds->basefilepath);
	AppendString(&ds->rawdatapath,"\\",-1);
	AppendString(&ds->rawdatapath,fullname,-1);
	free(fullname);
	if (FileExists (ds->rawdatapath, &fileSize)){
		if(ds->overwrite_files){
		//clear dir
			pathName=StrDup(ds->rawdatapath);
			AppendString(&pathName, "*.*", -1);	//create file name
			DeleteFile(pathName); 
			free(pathName);
		}
		else {
		//create unique dir
			uniquedir=FALSE;
			while (!uniquedir){
				pathName=StrDup(ds->rawdatapath); 
				dircounter++;
				dirctrstr=malloc(MAXBASEFILEPATH*sizeof(char));
				Fmt(dirctrstr,"%s<_%i",dircounter);
				AppendString(&pathName,dirctrstr,-1);
				OKfree(dirctrstr); 
				if (FileExists (pathName, &fileSize)!=1) {
					uniquedir=TRUE;
					OKfree(ds->rawdatapath);    
					ds->rawdatapath=StrDup(pathName);
					err=MakeDir(ds->rawdatapath);
				}
				free(pathName);
			}
			
		}
	}
	else  err=MakeDir(ds->rawdatapath);  
		
	return err;
	
}


char* CreateFullIterName(Iterator_type*		currentiter)
{
	
	Iterator_type* 	  childiter;
	Iterator_type* 	  parentiter;
	char*			  name;
	char*			  fullname; 
	char*			  tcname;
	size_t			  iteridx;
	
	if (currentiter==NULL) return NULL;
	
	parentiter = GetIteratorParent(currentiter);
	if (parentiter==NULL) return NULL;  //no parent
	tcname=GetCurrentIterationName(parentiter);
	iteridx=GetCurrentIterationIndex(parentiter);
	fullname=malloc(MAXBASEFILEPATH*sizeof(char));  
	Fmt (fullname, "%s<%s[w3]#%i",tcname,iteridx); 
	while (parentiter!=NULL) {  
		childiter=parentiter;
		parentiter = GetIteratorParent(childiter);
		if (parentiter==NULL) return fullname;  //no parent, return current name
		tcname=GetCurrentIterationName(parentiter);
		iteridx=GetCurrentIterationIndex(parentiter);
		name=malloc(MAXBASEFILEPATH*sizeof(char));
		Fmt (name, "%s<%s[w3]#%i",tcname,iteridx);    		
		AddStringPrefix (&fullname,"_",-1);    
		AddStringPrefix (&fullname,name,-1);
		OKfree(tcname);
		OKfree(name);
	}
	return fullname;	
	
}



static int Load (DAQLabModule_type* mod, int workspacePanHndl)
{
	DataStorage_type* 	ds 			= (DataStorage_type*) mod;
	int 				error		= 0; 
	int 				numListLines=5;

	// load main panel
	ds->mainPanHndl = LoadPanel(workspacePanHndl, UI_DataStorage, DSMain);
	
	// add module's task controller to the framework
	DLAddTaskController((DAQLabModule_type*)ds, ds->taskController);
	
	// connect module data and user interface callbackFn to all direct controls in the panel
	SetCtrlsInPanCBInfo(mod, ((DataStorage_type*)mod)->uiCtrlsCB, ds->mainPanHndl);
	
	SetCtrlVal(ds->mainPanHndl,DSMain_STRING,ds->basefilepath);
	
	
	DisplayPanel(ds->mainPanHndl);
	//default settings:

	TaskControlEvent(ds->taskController, TASK_EVENT_CONFIGURE, NULL, NULL);


Error:

	return error;

}




//-----------------------------------------
// DataStorage Task Controller Callbacks
//-----------------------------------------

 /* 
static int ConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
		
	return 0;
}

static int UnConfigureTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	return 0;
}

static void	IterateTC	(TaskControl_type* taskControl,  BOOL const* abortIterationFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	
	//return immediate
	TaskControlIterationDone (taskControl, 0, NULL,FALSE);

}

static void AbortIterationTC (TaskControl_type* taskControl, BOOL const* abortFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);    
}



static int	StartTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	TaskControl_type*		parent;
	char*					uiparentname;
	

	 
	return 0;
}

static int DoneTC (TaskControl_type* taskControl,  BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return 0;
}

static int StoppedTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return 0;
}
*/  
static int ResetTC (TaskControl_type* taskControl, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);

	return 0;
}

static void	DimUITC (TaskControl_type* taskControl, BOOL dimmed)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	
	if (dimmed) {
		//create a new data directory
		OKfree(ds->rawdatapath);
		CreateRawDataDir(ds,taskControl);
	}
}

 /*
static void	TCActive (TaskControl_type* taskControl, BOOL UITCActiveFlag)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
}
 */

static void	ErrorTC (TaskControl_type* taskControl, int errorID, char* errorMsg)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl);
	int error=0;


	// print error message
	DLMsg(errorMsg, 1);
}
 /*
static int ModuleEventHandler (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive,  void* eventData, BOOL const* abortFlag, char** errorInfo)
{
	DataStorage_type* 		ds 			= GetTaskControlModuleData(taskControl); 
	
	return 0;
}

 */


static void	RedrawDSPanel (DataStorage_type* ds)
{
	size_t		nChannels		= 0;
	int			menubarHeight	= 0;
	int			chanPanHeight	= 0;
	int			chanPanWidth	= 0;
	int			taskPanWidth	= 0;

	// count the number of channels in use
	if (ds->channels) nChannels = ListNumItems(ds->channels); 
	
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
	DS_Channel_type**	chanPtr; 
	char				buff[DAQLAB_MAX_VCHAN_NAME + 50];  
	char*				vChanName;
	char				channame[DAQLAB_MAX_VCHAN_NAME];
	int 				numitems;
	int 				checked;
	int 				treeindex;
	int 				i;
	int 				channr=0;
	int					itemIndex;
	int 				reply;
	char*				currentbasepath;
	
	
	switch (event) {
			
		case EVENT_COMMIT:
			
			switch (control) {
					case DSMain_Channels: 
					break;
					
					case DSMain_COMMANDBUTTON_ADD:
					
						
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
						chan = init_DS_Channel_type(ds, panel, vChanName);
		
						// register VChan with DAQLab
						DLRegisterVChan((DAQLabModule_type*)ds, (VChan_type*)chan->VChan);
						AddSinkVChan(ds->taskController, chan->VChan, DataReceivedTC);

						// update main panel
						RedrawDSPanel(ds);
					break;
					
					case DSMain_COMMANDBUTTON_REM:
						GetNumListItems(panel, DSMain_Channels,&numitems);    
						for(treeindex=0;treeindex<numitems;treeindex++){
							IsListItemChecked(panel,DSMain_Channels,treeindex, &checked);      
							if (checked==1) {
					   			// get channel name
								GetLabelFromIndex (panel, DSMain_Channels, treeindex, channame); 
								//get channel from list
								if (ds->channels) {
									numitems = ListNumItems(ds->channels); 
										for ( i= 1; i <= numitems; i++) {
											chanPtr = ListGetPtrToItem(ds->channels, i);
											if (chanPtr!=NULL) chan=*chanPtr;
											if (chan!=NULL) {
												if (strcmp(GetVChanName(chan->VChan),channame)==0){   
													//strings are equal; channel found
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
						currentbasepath=malloc(MAX_PATHNAME_LEN*sizeof(char));
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
	}
	return 0;
}


int SaveImage(Image* image,DataStorage_type* ds,char* vChanname,int iterationnr)
{
	int 			err=0;
	TIFFFileOptions options;
	char *			fileName;
	char 			buf[MAX_PATHNAME_LEN];   
	
	Fmt (buf, "%s<%s#%i[w4p0]",vChanname,iterationnr);    
	//append iteration number to file name
	fileName=StrDup(ds->basefilepath);
	AppendString(&fileName, buf, -1);	//create file name
	AppendString(&fileName, ".tif", -1);		 

	options.photoInterp=IMAQ_BLACK_IS_ZERO;
	options.compressionType=IMAQ_NO_COMPRESSION;
 	err=imaqWriteTIFFFile(image,fileName, &options, NULL); 
	
	free(fileName);
	
	return err;

}


static int DataReceivedTC (TaskControl_type* taskControl, TaskStates_type taskState, BOOL taskActive, SinkVChan_type* sinkVChan, BOOL const* abortFlag, char** errorInfo)  
{
	
	DataStorage_type*	ds					= GetTaskControlModuleData(taskControl);
	FCallReturn_type*	fCallReturn			= NULL;
	unsigned int		nSamples;
	int					error;
	DataPacket_type**	dataPackets			= NULL;
	unsigned short int* shortDataPtr		= NULL;
	size_t				nPackets;
	size_t				nElem;
	char*				sinkVChanName		= GetVChanName((VChan_type*)sinkVChan);
	SourceVChan_type*   sourceVChan			= GetSourceVChan((VChan_type*)sinkVChan); 
	char*				sourceVChanName		= GetVChanName((VChan_type*)sourceVChan);  
	char* 				rawfilename; 
	int 				filehandle;
	int 				elementsize=2;
	char* 				errMsg 				= StrDup("Writing data to ");
	char				cmtStatusMessage[CMT_MAX_MESSAGE_BUF_SIZE];
	void*				dataPacketDataPtr;
	DLDataTypes			dataPacketType; 
	Image*				image;
	size_t 				i;
	DS_Channel_type*	chan; 
	DS_Channel_type**	chanPtr; 
	int 				numitems;
	Iterator_type*		currentiter;
			
			
	if (ds->channels) {
		numitems = ListNumItems(ds->channels); 
		for ( i= 1; i <= numitems; i++) {
			chanPtr = ListGetPtrToItem(ds->channels, i);
			if (chanPtr!=NULL) chan=*chanPtr;
			if (chan!=NULL) {
				if (strcmp(GetVChanName(chan),sinkVChanName)==0){   
					//strings are equal; channel found
					break;
				}
			}
		}
	}
			
	// get all available data packets
	errChk( GetAllDataPackets(sinkVChan, &dataPackets, &nPackets, &errMsg) );
	
				
	for (i= 0; i < nPackets; i++) {
		if (dataPackets[i]==NULL){
			ReleaseDataPacket(&dataPackets[i]); 
		}
		else{
			dataPacketDataPtr = GetDataPacketPtrToData(dataPackets[i], &dataPacketType); 
			currentiter=GetDataPacketCurrentIter(dataPackets[i]);
			switch (dataPacketType) {
				case DL_Waveform_UShort:
					shortDataPtr = GetWaveformPtrToData(*(Waveform_type**)dataPacketDataPtr, &nElem);
						
					//test
					rawfilename=malloc(MAXCHAR*sizeof(char)); 
					OKfree(ds->name);
					ds->name=CreateFullIterName(currentiter);
					Fmt (rawfilename, "%s<%s\\%s_%s#%d.bin", ds->rawdatapath,ds->name,sourceVChanName,GetCurrentIterationIndex(currentiter));  
					filehandle=OpenFile (rawfilename, VAL_WRITE_ONLY, VAL_APPEND, VAL_BINARY);
					if (filehandle<0) {
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file open failed", -1); 
						
					}
					error=WriteFile (filehandle, shortDataPtr, nElem*elementsize);
					if (error<0) {
						AppendString(&errMsg, sinkVChanName, -1); 
						AppendString(&errMsg, " failed. Reason: file write failed", -1); 
					
					}
					free(rawfilename);
					CloseFile(filehandle);
				break;
				case DL_Image_NIVision:
					//get the image
				//	GetWaveformDataPtr(*(Waveform_type**)dataPacketDataPtr, &nElem);
		//			SaveImage(image,ds,sourceVChanName,chan->iteration++);
				break;
			}
			ReleaseDataPacket(&dataPackets[i]);
		}
	}
		
	 
	OKfree(dataPackets);				
			
			
	
Error:
	
	OKfree(sinkVChanName);
	OKfree(sourceVChanName); 
	OKfree(errMsg);
	return error;
}
